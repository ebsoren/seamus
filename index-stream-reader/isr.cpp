#include "isr.h"
#include "lib/logger.h"

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
    if (fseek(fd, INDEX_DICTIONARY_SIZE + urls_size, SEEK_SET) != 0) logger::warn("Error when initializing %s: Seek to dictionary failed.", path);

    uint64_t posting_list_offset;

    do {
        fgets(buff, sizeof(buff), fd);
        
        // Copy from end of buffer minus 1B for \n - 4B for offset to get start of offset
        memcpy(&posting_list_offset, buff + strlen(buff) - (1 + sizeof(uint64_t)), sizeof(uint64_t));

        // Insert offset into dictionary
        dictionary.insert(string(buff, strlen(buff) - (1 + sizeof(uint64_t))), posting_list_offset);
    } while (strcmp(buff, "\n"));

    // Now at the top of the skip list

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

// TODO: Refactor this to take in a LoadedIndex object instead of chunk_path
IndexStreamReader::IndexStreamReader(string word, string chunk_path) : word(move(word)) {
    struct stat buff;
    if (stat(chunk_path.data(), &buff) == -1) {
        logger::warn("ISR couldn't open. %s didn't exist.", chunk_path);
        return;
    }

    // rb: Read as a Binary file
    if ((fd_ = fopen(chunk_path.data(), "rb")) == nullptr) {
        logger::warn("ISR failed to open %s", chunk_path);
        return;
    }

    // Read the size of ID->URL map and skip over it
    uint64_t urlmap_size;
    fread(&urlmap_size, sizeof(uint64_t), 1, fd_);
    if (fseek(fd_, urlmap_size + 1, SEEK_CUR) != 0) { // +1 for the newline delimiter after the size
        logger::warn("Fseek to skip over URL map failed when parsing %s.", chunk_path);
        return;
    } 

    // Now at the dictionary lookup table
    // Skip to the letter for this word to see offset to go to
    long int dict_start = ftell(fd_);
    int dict_index = word.data()[0] - 'a';
    int dict_offset = dict_index * (64 + 1);
    if (fseek(fd_, dict_offset, SEEK_CUR) != 0) {
        logger::warn("Fseek to index into dictionary failed when parsing %s.", chunk_path);
        return;
    }

    char ch;
    uint64_t letter_offset;
    fread(&ch, sizeof(char), 1, fd_);
    if (ch != word.data()[0]) logger::warn("Seeked to wrong letter in dictionary for %s.", word);
    fread(&letter_offset, sizeof(uint64_t), 1, fd_);

    // Seek ahead to first word in the dictionary that starts with this letter
    // The letter offset is from the START of the dictionary, so we subtract the amount we've already jumped
    fseek(fd_, letter_offset - dict_offset, SEEK_CUR);

    // Go through words starting with this letter in dictionary until desired word is found
    char word_buff[256];
    uint64_t word_offset;
    while (fgets(word_buff, sizeof(buff), fd_)) {
        // Word found,
        if (!memcmp(word_buff, word.data(), word.size())) break;
    }

    // Copy the offset to that word
    // +word.size() skips the word, +1 skips the space
    memcpy(&word_offset, word_buff + word.size() + 1, sizeof(uint64_t));

    // Jump to posting list using that offset (which is from start of dictionary)
    // SEEK_SET starts at beginning of file
    fseek(fd_, dict_start + word_offset, SEEK_SET);

    fread(&n_posts, sizeof(uint64_t), 1, fd_);
    fread(&ch, sizeof(char), 1, fd_); // Skip the space
    fread(&n_docs, sizeof(uint64_t), 1, fd_);
    fread(&ch, sizeof(char), 1, fd_); // Skip the newline

    postings_start = ftell(fd_);

    // Starting of posting list, num_docs, num_posts all set
    // fd_ currently sits at the start of posting list (aka first skip list entry)
}