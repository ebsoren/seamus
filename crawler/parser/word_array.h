#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <initializer_list>

#include <stdlib.h>

#include "../../lib/io.h"


// Array specifically for storing words with added method push_back providing easy use
template<size_t MAX_MEMORY>
class word_array {
public:
    int fd_ = -1;
    static constexpr char DEFAULT_DELIM = '\n';
    word_array()
        : fd_(-1) {}

    word_array(int fd)
        : fd_(fd) {}

    void push_back(const char *start, size_t len, char delim = DEFAULT_DELIM) {
        if (len >= MAX_MEMORY - size_ - 1) {
            seamus_write(fd_, data_, size_);
            size_ = 0;
        }
        memcpy(data_ + size_, start, len);
        size_ += len;
        if (delim != '\0') {
            data_[size_++] = delim;
        }
    }

    void flush() {
        if (size_ > 0) {
            seamus_write(fd_, data_, size_);
            size_ = 0;
        }
    }

    // Convert the contents of a given segment of the data array to lowercase
    // Converts the entire array if no args are given
    void case_convert(int start = 0, int end = MAX_MEMORY) {
        if (end == MAX_MEMORY) end = size_;
        while (start < end) {
            *(data_ + start) = tolower(*(data_ + start));
            start++;
        }
    }

    char *data() { return data_; }

    const char *data() const { return data_; }

    char *begin() { return data_; }
    char *end() { return data_ + MAX_MEMORY; }

    constexpr std::size_t size() const { return size_; }

    char &operator[](std::size_t i) { return data_[i]; }

    const char &operator[](size_t i) const { return data_[i]; }

private:
    char data_[MAX_MEMORY];
    size_t size_ = 0;
};
