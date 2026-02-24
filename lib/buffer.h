#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>

// BLOCK_SIZE HTML is read into
static const uint32_t CAPACITY = 500000;

class buffer {
public:
    // Default constructor
    buffer() {
        size_ = 0;
        init_data_();
    }

    // No copying or moving
    buffer(const buffer &) = delete;
    buffer &operator=(const buffer &) = delete;
    buffer(buffer &&) = delete;
    buffer &operator=(buffer &&) = delete;

    // Destructor
    ~buffer() { free(data_); }


    // Random access operator
    char &operator[](size_t i) { return data_[i]; }

    const char &operator[](size_t i) const { return data_[i]; }

    // Various access methods

    char &front() { return data_[0]; }
    char &back() { return data_[size_ - 1]; }

    char *data() { return data_; }

    const char &front() const { return data_[0]; }
    const char &back() const { return data_[size_ - 1]; }

    const char *data() const { return data_; }

    char *begin() { return data_; }
    char *end() { return data_ + size_; }

    const char *begin() const { return data_; }
    const char *end() const { return data_ + size_; }

    // Erase (TODO)
    // Remove a part of the buffer, and return a reference to the next character after the deleted section,
    // which will be concatenated to the next deleted section (or to the end of the buffer)
    void erase(char *loc) {
        if (size_ == 0) return;   // Uhh throw smth or smth idk should never be doing this (TODO)
        while (loc < (data_ + size_ - 1)) {
            *loc = *(loc + 1);
            loc++;
        }
        size_--;
    }

    // Read up to CAPACITY bytes into the buffer. Returns 1 on successful read,
    // 0 on empty read, and -1 on error
    int8_t read(int fd) {
        uint32_t bytes_read = 0;
        ssize_t bytes_in;
        while ((bytes_in = recv(fd, data_ + bytes_read, CAPACITY - bytes_read, 0)) > 0) {
            bytes_read += bytes_in;
        }
        if (bytes_in < 0) return -1;
        if (bytes_read == 0) return 0;
        size_ = bytes_read;
        return 1;
    }

    // Write contents to disk and reset buffer
    void write_to_disk(char *filepath) {
        int fd = open(filepath, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd == -1) return;   // Uhh throw something ig (TODO)
        ssize_t w = write(fd, data_, size_);
        if (w == -1) return; // Throw something idk (TODO)
        close(fd);

        size_ = 0;
    }


private:
    char *data_;    // Underlying data
    size_t size_;   // number of elements

    void init_data_() {
        data_ = static_cast<char *>(malloc(CAPACITY));
        if (data_ == nullptr) return; // Throw something prob (TODO)
    }
};

/*

psuedocode for usage

get network sock and shi

buffer b

while(int8_t x = b.read(fd)) {
    if (x < 0) return; // Network error
    parse(b);
    b.write_to_disk(filepath);
}

close(fd);


*/