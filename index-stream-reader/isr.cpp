#include "isr.h"
#include "lib/logger.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

LoadedIndex::LoadedIndex(string path) {
    urls.resize(DOCS_PER_INDEX_CHUNK);
    dictionary.reserve(DOCS_PER_INDEX_CHUNK);

    // TODO: This is huge. Is heap allocating like this alright?
    posting_list_ = new uint8_t[POSTING_LIST_BUFFER_SIZE];

    FILE* fd;
    if ((fd = fopen(path.data(), "rb")) == nullptr) {
        logger::warn("Failed to open %s", path);
        return;
    }
    
    // Populate the URL vector
    uint64_t urls_size;
    fread(&urls_size, sizeof(uint64_t), 1, fd);

    char url_buff[4096];
    uint64_t bytes_read = 0;
    while (bytes_read < urls_size) {
        fgets(url_buff, sizeof(url_buff), fd);
        bytes_read += strlen(url_buff);
        // TODO: Parse <32b ID> <varlen URL>\n and add to urls vector 
    }

    // Populate the dictionary

    // Populate the posting list

    fclose(fd);
}

LoadedIndex::~LoadedIndex() {
    delete[] posting_list_;
}

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