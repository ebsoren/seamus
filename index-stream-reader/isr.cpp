#include "isr.h"
#include "lib/logger.h"
#include "lib/utf8.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

LoadedIndex::LoadedIndex(string path) {
    urls.resize(DOCS_PER_INDEX_CHUNK);
    dictionary.reserve(DOCS_PER_INDEX_CHUNK);

    FILE* fd;
    if ((fd = fopen(path.data(), "rb")) == nullptr) {
        logger::warn("Failed to open %s", path);
        return;
    }
    
    // Populate the URL vector
    uint64_t urls_size;
    fread(&urls_size, sizeof(uint64_t), 1, fd);

    char buff[4096];
    uint64_t bytes_read = 0;

    uint32_t id;
    while (bytes_read < urls_size) {
        fgets(buff, sizeof(buff), fd);
        bytes_read += strlen(buff);

        // Parse <32b ID> <varlen URL>\n and add to urls vector
        // The URL starts after 4B Id and 1B char, and the total length is 4B + 1B + strlen + 1B
        memcpy(&id, buff, sizeof(uint32_t));
        urls[id] = string(buff + sizeof(uint32_t) + 1, strlen(buff) - sizeof(uint32_t) - 2);
    }
    
    // Should now be at the top of the dict table of contents
    // Validate this by checking that we see the char 'a'
    char ch;
    fread(&ch, sizeof(char), 1, fd);
    if (ch != 'a') logger::warn("Error when initializing %s: Not at expected start of dict contents.", path);

    // Populate the dictionary
    // Seek precisely to start of dictionary
    if (fseek(fd, INDEX_DICTIONARY_TOC_SIZE + urls_size, SEEK_SET) != 0) logger::warn("Error when initializing %s: Seek to dictionary failed.", path);

    uint64_t posting_list_offset;

    do {
        fgets(buff, sizeof(buff), fd);
        dictionary_size += strlen(buff);
        
        // Copy from end of buffer minus 1B for \n - 4B for offset to get start of offset
        memcpy(&posting_list_offset, buff + strlen(buff) - (1 + sizeof(uint64_t)), sizeof(uint64_t));

        // Insert offset into dictionary
        dictionary.insert(string(buff, strlen(buff) - (1 + sizeof(uint64_t))), posting_list_offset);
    } while (strcmp(buff, "\n"));
    dictionary_size++;

    // Now at the top of the posting lists

    // Populate the posting list
    // TODO: This is huge. Is heap allocating like this alright?
    posting_list_ = new uint8_t[POSTING_LIST_BUFFER_SIZE];

    // Read until the end of file, and assert that end of file was reached
    fread(posting_list_, sizeof(uint8_t), UINT64_MAX, fd);

    if (feof(fd) == 0) logger::warn("Failed to reach end of file when loading %s posting list.", path);

    fclose(fd);
}

LoadedIndex::~LoadedIndex() {
    delete[] posting_list_;
}

IndexStreamReader::IndexStreamReader(string word, LoadedIndex* index) : word(move(word)), index(index) {
    // Get offset of the word from start of posting lists
    // Postings list don't include the dictionary (but the offset does), so subtract dictionary size
    uint64_t offset = index->dictionary[word] - index->dictionary_size;

    // Jump to the word's posting list using that offset (which is from )
    curr_loc_ = postings_start_= index->posting_list_ + offset;

    // TODO: Check these casts...
    n_posts = static_cast<uint64_t>(*curr_loc_);
    curr_loc_ += sizeof(uint64_t) + 1; // skip over the number and the space
    n_docs = static_cast<uint64_t>(*curr_loc_);
    curr_loc_ += sizeof(uint64_t) + 1; // skip over the number and the newline
    
    // Starting of posting list, num_docs, num_posts all set
}

const inline post IndexStreamReader::loc() {
    return post{doc_offset_, loc_offset_};
}

post IndexStreamReader::advance() {
    if (posts_consumed_ == n_posts) return post{0, 0};

    posts_consumed_++;
    uint32_t loc_or_flag = ReadUtf8(&curr_loc_, nullptr);
    if (loc_or_flag > 0) {
        loc_offset_ += loc_or_flag;
        return post{doc_offset_, loc_offset_};
    }

    // If we read 0, that was the flag for a new doc
    doc_offset_ += ReadUtf8(&curr_loc_, nullptr); // Add to doc offset
    loc_offset_= ReadUtf8(&curr_loc_, nullptr); // Reset loc offset
    return post{doc_offset_, loc_offset_};
}

post IndexStreamReader::advance_to(uint32_t doc) {
    while (doc_offset_ < doc) {
        post p = advance();
        if (p.doc == 0) return p;
    }
    return post{doc_offset_, loc_offset_};
}
