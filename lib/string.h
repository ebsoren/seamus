//
// Created by Aiden Mizhen on 2/13/26.
//

#pragma once

#include <algorithm>
#include <cassert>
#include <ostream>
#include <cstring>

class string;

class string_view {
private:
    const char* data_;
    size_t len;

public:
    string_view(const char* source, size_t len) : data_{source}, len{len} {}

    [[nodiscard]] string to_string() const;

    [[nodiscard]] string_view substr(size_t start, size_t length) const {
        assert(start+length<=len);
        return string_view{data_ + start, length};
    }

    [[nodiscard]] size_t size() const {
        return len;
    }

    const char& operator[](const size_t i) const {
        assert(i < len);
        return data_[i];
    }

    bool operator==(const string_view &other) const {
        if (&other == this) return true;

        if (len != other.size())  return false;

        if (data_==other.data_) return true;

        return memcmp(data_, other.data_, len) == 0;
    }

    [[nodiscard]] const char* data() const noexcept {
        return data_;
    }

    friend bool operator==(const string_view& lhs, const char* rhs) {
        if (rhs == nullptr) return false;

        const size_t rhs_len = strlen(rhs);

        if (lhs.size() != rhs_len) return false;

        if (lhs.data() == rhs) return true;

        return memcmp(lhs.data(), rhs, rhs_len) == 0;
    }

    friend bool operator==(const char* lhs, const string_view& rhs) {
        return rhs == lhs;
    }
};


class string {
public:
    static constexpr size_t MAX_SHORT_LENGTH = 14;

private:
    static constexpr unsigned char SHORT_FLAG = 1;
    static constexpr unsigned char LONG_FLAG  = 0;
    static constexpr unsigned char FLAG_MASK  = 1;

    union State {
        struct {
            size_t flag_and_size; // LSB must be zero, used as flag
            char* data;
        } l;
        struct {
            unsigned char flag_and_size; // least significant bit = flag, upper 7 = str size
            char data[MAX_SHORT_LENGTH + 1]; // 14 bits short string length + 1 null terminator
        } s;
    } state;

    // To determine short or long state
    [[nodiscard]] bool is_short() const {
        return state.s.flag_and_size & FLAG_MASK;
    }

    // Moves the contents of another string to this
    void move_from(string&& other) noexcept {
        this->state = other.state;
        // Mark other as empty short string so destructor isn't ran
        other.state = {};
        other.state.s.flag_and_size = SHORT_FLAG;
    }


    struct UninitializedTag {};

    // FOR INTERNAL USE ONLY
    // TO INITIALIZE AN EMPTY STRING WITH GIVEN LENGTH
    string(size_t len, UninitializedTag) : state{}  {
        if (len <= MAX_SHORT_LENGTH) {
            // Short String
            state.s.flag_and_size = static_cast<unsigned char>(len << 1 | SHORT_FLAG);
        } else {
            // Long String
            state.l.flag_and_size = ((len<< 1) | LONG_FLAG); // Set flag to zero
            state.l.data = static_cast<char*>(::operator new(len + 1));
            state.l.data[len] = '\0';
        }
    }

public:
    explicit string (const char *c_str) : string{c_str, strlen(c_str)} {}

    template<size_t N>
    explicit string(const char (&c_str)[N]) : string{c_str, N - 1} {}


    explicit string(const char* c_str, size_t len) : string{len, UninitializedTag{}} {
        assert(c_str != nullptr);
        if (len <= MAX_SHORT_LENGTH) {
            // Short string
            memcpy(state.s.data, c_str, len);
        } else {
            // Long string
            memcpy(state.l.data, c_str, len);
            state.l.data[len] = '\0';
        }
    }


    ~string() {
        if (!is_short()) {
            size_t actual_allocation_size = size() + 1;
            ::operator delete(state.l.data, actual_allocation_size);
        }
    }

    // No copying
    string(const string& other) = delete;

    // No copying
    string& operator=(const string& other) = delete;

    // No nullptrs
    explicit string(std::nullptr_t) = delete;

    string(string&& other) noexcept /* NOLINT */ {
        move_from(static_cast<string&&>(other));
    }


    explicit string(uint32_t n) : state{} {
        // At most 4 Billion = 10 digits => short string
        if (n == 0) {
            state.s.data[0] = '0';
            state.s.flag_and_size = static_cast<unsigned char>((1 << 1) | SHORT_FLAG); // Len 1, Flag 1
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
        memcpy(state.s.data,tmp+i, len + 1);
        state.s.flag_and_size = static_cast<unsigned char>((len << 1) | SHORT_FLAG);
    }


    string& operator=(string&& other) noexcept {
        if (this != &other) {
            if (!is_short()) {
                ::operator delete(state.l.data);
            }

            move_from(static_cast<string&&>(other));
        }
        return *this;
    }


    [[nodiscard]] size_t size() const {
        //Remove flag bit
        if (is_short()) [[likely]]{
            return static_cast<size_t>(state.s.flag_and_size >> 1);
        }
        return state.l.flag_and_size >> 1;
    }


    const char& operator[](const size_t i) const {
        assert(i < size());
        if (is_short()) [[likely]]{
            return state.s.data[i];
        }
        return state.l.data[i];
    }

    [[nodiscard]] const char* data() const noexcept {
        return is_short() ? state.s.data : state.l.data;
    }


    [[nodiscard]] string_view str_view(const size_t pos, const size_t len) const {
        assert(pos <= size());
        assert(pos + len <= size());

        if (is_short()) [[likely]]{
            return {state.s.data + pos, len};
        }
        return {state.l.data + pos, len};
    }

private:
    // Overloads to resolve size for different types in join()
    static size_t get_len(const string& str) { return str.size(); }
    static size_t get_len(const string_view& str) { return str.size(); }
    template <size_t N>
    static size_t get_len(const char (&)[N]) { return N - 1; }

    // Overloads to resolve the data pointer
    static const char* get_ptr(const string& str) { return str.data(); }
    static const char* get_ptr(const string_view& str) { return str.data(); }
    template <size_t N>
    static const char* get_ptr(const char (&str)[N]) { return str; }

public:
    template <size_t N, typename... Args>
    static string join(const char (&delimit)[N], const Args&... args) {
        constexpr size_t delim_len = N - 1;
        constexpr size_t num_args = sizeof...(Args);

        if constexpr (num_args == 0) {
            return string("");
        } else {
            size_t total_len = (get_len(args) + ...) + delim_len * (num_args - 1);

            string out{total_len, UninitializedTag{}};
            char* out_data = out.is_short() ? out.state.s.data : out.state.l.data;

            size_t pos = 0;
            bool is_first = true;

            auto append = [&pos, &out_data, &delimit, &is_first](const auto& arg) -> void {
                if (not is_first) {
                    memcpy(&out_data[pos], delimit, delim_len);
                    pos += delim_len;
                } else {
                    is_first = false;
                }

                size_t arg_len = get_len(arg);
                if (arg_len > 0) {
                    memcpy(out_data + pos, get_ptr(arg), arg_len);
                    pos += arg_len;
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

        if (is_short()) [[likely]]{
            return memcmp(&state, &other.state, 16) == 0;
        } else {
            if (state.l.data == other.state.l.data) return true;
            return memcmp(state.l.data, other.state.l.data, len) == 0;
        }
    }


    friend bool operator==(const string& lhs, const char* rhs) {
        if (rhs == nullptr) return false; // Safety first

        size_t rhs_len = strlen(rhs);

        if (lhs.size() != rhs_len) return false;

        if (lhs.is_short()) {
            return memcmp(lhs.state.s.data, rhs, rhs_len) == 0;
        } else {
            if (lhs.state.l.data == rhs) {
                return true;
            }
            return memcmp(lhs.state.l.data, rhs, rhs_len) == 0;
        }
    }

    friend bool operator==(const char* lhs, const string& rhs) {
        return rhs == lhs;
    }
};


inline std::ostream& operator<<(std::ostream& os, const string_view& str) {
    return os.write(str.data(), static_cast<std::streamsize>(str.size()));
}

inline std::ostream& operator<<(std::ostream& os, const string& str) {
    return os.write(str.data(), static_cast<std::streamsize>(str.size()));
}



inline string string_view::to_string() const {
    return string(data(), len);
}


inline bool operator==(const string& lhs, const string_view& rhs) {
    if (lhs.size() != rhs.size()) return false;

    if (rhs.data() == lhs.data()) return true;
    return memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}


inline bool operator==(const string_view& lhs, const string& rhs) {
    return rhs == lhs;
}