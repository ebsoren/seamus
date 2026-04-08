#include "isr.h"
#include "lib/logger.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

IndexStreamReader::IndexStreamReader(string word, string chunk_path) : word(move(word)) {
    struct stat buff;
    int fd_;
    if (stat(chunk_path.data(), &buff) == -1) {
        logger::warn("ISR couldn't open. %s didn't exist.", chunk_path);
        return;
    }

    if (fd_ = open(chunk_path.data(), O_RDONLY, 0444) == -1) {
        logger::warn("ISR failed to open %s", chunk_path);
        return;
    }

    // MAP_PRIVATE means writes don't go to the underlying file. That's fine; we're not writing
    void* mapped_file = mmap(NULL, buff.st_size, PROT_READ, MAP_PRIVATE, fd_, 0);
    close(fd_);

    if (mapped_file == MAP_FAILED) {
        logger::warn("Memory mapping %s failed.", chunk_path);
        return;
    }

    // TODO: Move the ISR to the start of the posting locations

}