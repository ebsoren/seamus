#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>

#include "io.h"

class buffer {
public:
    // Can change later - this is 32KB
    static constexpr uint32_t CAPACITY = 32 * 1024;

    buffer()
        : size_(0)
        , data_(new char[CAPACITY])
        , owned_data_(data_) {}

    ~buffer() { delete[] owned_data_; }

    buffer(const buffer &) = delete;
    buffer &operator=(const buffer &) = delete;
    buffer(buffer &&) = delete;
    buffer &operator=(buffer &&) = delete;

    char &operator[](size_t i) { return data_[i]; }
    const char &operator[](size_t i) const { return data_[i]; }

    char &front() { return data_[0]; }
    char &back() { return data_[size_ - 1]; }
    const char &front() const { return data_[0]; }
    const char &back() const { return data_[size_ - 1]; }

    char *data() { return data_; }
    const char *data() const { return data_; }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    char *begin() { return data_; }
    char *end() { return data_ + size_; }
    const char *begin() const { return data_; }
    const char *end() const { return data_ + size_; }

    void clear() {
        data_ = owned_data_;
        size_ = 0;
    }

    // Read from fd, appending after any existing data. Returns bytes read
    // on success, 0 on empty read, -1 on error.
    ssize_t read(int fd) {
        ssize_t bytes_read = seamus_read(fd, data_ + size_, CAPACITY - size_);
        if (bytes_read <= 0) {
            return bytes_read;
        }
        size_ += bytes_read;
        return bytes_read;
    }

    void set_buffer(char* new_data, size_t new_size) {
        data_ = new_data;
        size_ = new_size;
    }


    void shift_data(size_t n) {

        // Shift unconsumed remainder to front
        if (n >= size_) {
            size_ = 0;
        } else {
            size_t remaining = size_ - n;
            memmove(data_, data_ + n, remaining);
            size_ = remaining;
        }
    }


private:
    size_t size_;
    char *data_;
    char *owned_data_;
};