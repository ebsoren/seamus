#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <iostream>

#include <stdlib.h>

#include "../lib/io.h"
#include "../lib/logger.h"


// Array specifically for storing words with added method push_back providing easy use
template <size_t MAX_MEMORY>
class buffer_array {
public:
    static constexpr char DEFAULT_DELIM = '\n';
    buffer_array() {}

    void push_back(const char *start, size_t len, char delim = DEFAULT_DELIM) {
        // Guard against wrapped-negative lengths from pointer arithmetic on malformed HTML
        if (len > MAX_MEMORY) {
            logger::warn("buffer_array::push_back: rejecting bogus len=%zu (likely negative pointer diff)", len);
            return;
        }
        if (this->size_ + len + 10 >= MAX_MEMORY) { // 10 to have space for /doc
            logger::debug("buffer_array flush triggered: size_=%zu, incoming len=%zu, capacity=%zu", this->size_, len,
                          MAX_MEMORY);
            flush();

            // After flush, if the single item still can't fit, truncate to avoid overflow
            if (len >= MAX_MEMORY - 1) {
                logger::warn("buffer_array: item too large for buffer (len=%zu, max=%zu), truncating", len, MAX_MEMORY);
                len = MAX_MEMORY - 2;
            }
        }
        if (start && len > 0) {
            memcpy(data_ + size_, start, len);
        }
        this->size_ += len;
        if (delim != '\0') {
            data_[size_++] = delim;
        }
    }
    virtual void flush() = 0;

    // Resets a word_array to be "dataless" without requiring reallocation of data_
    // TODO: Esben, am I correct that this works as intended/is sufficient?
    // Don't need to reset the actual data_ array, I believe. YES - Esben
    inline void reset() { this->size_ = 0; }

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


protected:
    char data_[MAX_MEMORY];
    size_t size_ = 0;
};

template <size_t MAX_MEMORY>
class word_array : public buffer_array<MAX_MEMORY> {
public:
    int fd_ = -1;

    word_array() = default;

    word_array(int fd)
        : fd_(fd) {}
    void flush() override {
        if (this->size_ > 0) {
            this->case_convert();
            seamus_write(fd_, this->data_, this->size_);
            this->size_ = 0;
        }
    }
};

template <size_t MAX_MEMORY>
class link_array : public buffer_array<MAX_MEMORY> {
public:
    void set_callback(std::function<void(link_array &)> func) { provided_flush_func = func; }
    link_array() = default;
    void flush() override {
        if (this->size_ > 0 && provided_flush_func) {
            provided_flush_func(*this);
        }
    }

    void push_docend() {
        constexpr const char tag[] = "\n</doc>\n";
        constexpr size_t tag_len = 8;
        if (this->size_ + tag_len <= MAX_MEMORY) {
            memcpy(this->data_ + this->size_, tag, tag_len);
            this->size_ += tag_len;
        } else {
            logger::warn("push_docend: no room in buffer (size_=%zu, capacity=%zu)", this->size_, MAX_MEMORY);
        }
    }


private:
    std::function<void(link_array &)> provided_flush_func;
};
