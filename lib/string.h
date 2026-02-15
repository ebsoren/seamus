//
// Created by Aiden Mizhen on 2/13/26.
//

#pragma once

#include <algorithm>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <ostream>

class string;

class string_view {
private:
    const char* data;
    size_t len;

public:
    string_view(const char* source, size_t len) : data{source}, len{len} {}

    [[nodiscard]] string to_string() const;

    [[nodiscard]] string_view substr(size_t start, size_t length) const {
        assert(start+length<=len);
        return string_view{data + start, length};
    }

    [[nodiscard]] size_t size() const {
        return len;
    }

    const char& operator[](const size_t i) const {
        assert(i < len);
        return data[i];
    }

    bool operator==(const string_view &other) const {
        if (&other == this) return true;

        if (len != other.size())  return false;

        if (data==other.data) return true;

        return memcmp(data, other.data, len) == 0;
    }


    friend bool operator==(const string& lhs, const string_view& rhs);
    friend bool operator==(const string_view& lhs, const string& rhs);


    friend bool operator==(const string_view& lhs, const char* rhs) {
        if (rhs == nullptr) return false;

        size_t rhs_len = strlen(rhs);

        if (lhs.size() != rhs_len) return false;

        if (lhs.data == rhs) return true;

        return memcmp(lhs.data, rhs, rhs_len) == 0;
    }

    friend bool operator==(const char* lhs, const string_view& rhs) {
        return rhs == lhs;
    }


    friend std::ostream& operator<<(std::ostream& os, const string_view& str) {
        return os.write(str.data, static_cast<long>(str.len));
    }
};


class string {
    friend class string_view;

public:
    static constexpr size_t MAX_SHORT_LENGTH = 14;

private:
    static constexpr unsigned char SHORT_FLAG = 1;
    static constexpr unsigned char LONG_FLAG  = 0;
    static constexpr unsigned char FLAG_MASK  = 1;

    union {
        struct {
            size_t flag_and_size; // LSB must be zero, used as flag
            char* data;
        } l;
        struct {
            unsigned char flag_and_size; // least significant bit = flag, upper 7 = str size
            char data[MAX_SHORT_LENGTH + 1]; // 14 bits short string length + 1 null terminator
        } s;
    };

    // To determine short or long state
    [[nodiscard]] bool is_short() const {
        return s.flag_and_size & FLAG_MASK;
    }

    // Moves the contents of another string to this
    void move_from(string&& other) noexcept {
        if (other.is_short()) {
            s.flag_and_size = other.s.flag_and_size;
            memcpy(s.data, other.s.data, MAX_SHORT_LENGTH + 1);
        } else {
            l.flag_and_size = other.l.flag_and_size;
            l.data = other.l.data;
        }
        // Mark as empty short string so destructor isn't ran
        other.s.flag_and_size = SHORT_FLAG;
        other.s.data[0] = '\0';
    }


    struct UninitializedTag {};

    // FOR INTERNAL USE ONLY
    // TO INITIALIZE AN EMPTY STRING WITH GIVEN LENGTH
    string(size_t len, UninitializedTag) /* NOLINT */ {
        if (len <= MAX_SHORT_LENGTH) {
            // Short String
            memset(s.data,0,MAX_SHORT_LENGTH + 1);
            s.flag_and_size = static_cast<unsigned char>(len << 1 | SHORT_FLAG);
            s.data[len] = '\0';
        } else {
            // Long String
            l.flag_and_size = ((len<< 1) | LONG_FLAG); // Set flag to zero
            l.data = new char[len+1];
            l.data[len] = '\0';
        }
    }

public:
    explicit string (const char *c_str) : string(c_str, c_str ? strlen(c_str) : 0) {}


    explicit string(const char* c_str, size_t len) : string(len, UninitializedTag{}) {
        assert(c_str != nullptr);
        if (len <= MAX_SHORT_LENGTH) {
            // Short string
            memcpy(s.data, c_str, len);
        } else {
            // Long string
            memcpy(l.data, c_str, len);
        }
    }


    ~string() {
        if (!is_short()) {
            delete[] l.data;
        }
    }

    // No copying
    string(const string& other) = delete;

    // No copying
    string& operator=(const string& other) = delete;

    string(string&& other) noexcept /* NOLINT */ {
        move_from(static_cast<string&&>(other));
    }


    explicit string (uint32_t n) /* NOLINT */ {
        // At most 4 Billion = 10 digits => short string
        memset(s.data,0,MAX_SHORT_LENGTH + 1);
        if (n == 0) {
            s.data[0] = '0';
            s.data[1] = '\0';
            s.flag_and_size = static_cast<unsigned char>((1 << 1) | SHORT_FLAG); // Len 1, Flag 1
            return;
        }

        char tmp[11];
        tmp[10] = '\0';
        int i = 10;

        while (n > 0) {
            tmp[--i] = static_cast<char>(n % 10 + '0');
            n /= 10;
        }

        const int len = 10-i;
        memcpy(s.data,tmp+i, len + 1);

        s.flag_and_size = static_cast<unsigned char>((len << 1) | SHORT_FLAG);
    }


    string& operator=(string&& other) noexcept {
        if (this != &other) {
            if (!is_short()) {
                delete[] l.data;
            }

            move_from(static_cast<string&&>(other));
        }
        return *this;
    }


    [[nodiscard]] size_t size() const {
        //Remove flag bit
        if (is_short()) {
            return static_cast<size_t>(s.flag_and_size >> 1);
        }
        return l.flag_and_size >> 1;
    }


    const char& operator[](const size_t i) const {
        assert(i < size());
        if (is_short()) {
            return s.data[i];
        }
        return l.data[i];
    }


    [[nodiscard]] string_view str_view(const size_t pos, const size_t len) const {
        assert(pos <= size());
        assert(pos + len <= size());

        if (is_short()) {
            return {s.data + pos, len};
        }
        return {l.data + pos, len};
    }


    template <size_t N, typename... Args>
    static string join(const char (&delimit)[N], const Args&... args) {
        constexpr size_t delim_len = N - 1;
        constexpr size_t num_args = sizeof...(Args);

        if constexpr (num_args == 0) {
            return string("");
        } else {
            auto get_len = [](const auto& arg) -> size_t {
                return arg.size() ;
            };

            size_t total_len = (get_len(args) + ...) + delim_len * (num_args - 1);

            string out{total_len, UninitializedTag{}};
            char* out_data = out.is_short() ? out.s.data : out.l.data;

            size_t pos = 0;
            bool is_first = true;
            auto append = [&pos, &out_data, &delimit, &is_first](const auto& arg) -> void {
                if (not is_first) {
                    memcpy(&out_data[pos], delimit, delim_len);
                    pos += delim_len;
                } else {
                    is_first = false;
                }
                if (arg.size() > 0) {
                    memcpy(&out_data[pos], &arg[0], arg.size());
                    pos += arg.size();
                }
            };

            (append(args), ...);

            return out;
        }
    }


    bool operator==(const string &other) const {
        if (&other == this) return true;

        const size_t len = size();
        if (len != other.size())  return false;

        if (is_short()) {
            return memcmp(s.data, other.s.data, len) == 0;
        } else {
            if (l.data == other.l.data) {
                return true;
            }
            return memcmp(l.data, other.l.data, len) == 0;
        }
    }


    friend bool operator==(const string& lhs, const char* rhs) {
        if (rhs == nullptr) return false; // Safety first

        size_t rhs_len = strlen(rhs);

        if (lhs.size() != rhs_len) return false;

        if (lhs.is_short()) {
            return memcmp(lhs.s.data, rhs, rhs_len) == 0;
        } else {
            if (lhs.l.data == rhs) {
                return true;
            }
            return memcmp(lhs.l.data, rhs, rhs_len) == 0;
        }
    }

    friend bool operator==(const char* lhs, const string& rhs) {
        return rhs == lhs;
    }


    friend std::ostream& operator<<(std::ostream& os, const string& str) {
        if (str.is_short()) {
            return os.write(str.s.data, static_cast<long>(str.size()));
        }
        return os.write(str.l.data, static_cast<std::streamsize>(str.size()));
    }

    friend bool operator==(const string& lhs, const string_view& rhs);
    friend bool operator==(const string_view& lhs, const string& rhs);
};


inline string string_view::to_string() const {
    return string(data, len);
}


inline bool operator==(const string& lhs, const string_view& rhs) {
    if (lhs.size() != rhs.size()) return false;

    if (lhs.is_short()) {
        if (rhs.data == lhs.s.data) return true;
        return memcmp(lhs.s.data, rhs.data, lhs.size()) == 0;
    }

    if (rhs.data == lhs.l.data) return true;
    return memcmp(lhs.l.data, rhs.data, lhs.size()) == 0;
}


inline bool operator==(const string_view& lhs, const string& rhs) {
    return rhs == lhs;
}