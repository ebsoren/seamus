#include "isr.h"
#include "lib/logger.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

IndexStreamReader::IndexStreamReader(string word, string chunk_path) : word(move(word)) {
    struct stat buff;
    FILE * fd_;
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

    // Seek ahead to first word that starts with this letter
    // The letter offset is from the START of the dictionary, so we subtract the amount we've already jumped
    fseek(fd_, letter_offset - dict_offset, SEEK_CUR);

    // TODO: Skip ahead to this word
}