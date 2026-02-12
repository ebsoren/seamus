// string.h
//
// Starter file for a string template
#pragma once

#include <cassert>
#include <cstddef>    // for size_t
#include <iostream>   // for ostream
#include <utility>

// Specify default length of string constructed with c string
const size_t DEFAULT_LENGTH = 16;


inline size_t min(size_t a, size_t b) {
    return a < b ? a : b;
}

inline size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

class string {
public:
    // Default Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Creates an empty string
    string() : sz(0), cap(0), data(nullptr)  {
        // Defaults
        alloc_(DEFAULT_LENGTH);
    }

    // Destructor
    // REQUIRES: Nothing
    // MODIFIES: Destroys *this
    // EFFECTS: Performs any neccessary clean up operations
    ~string() {
        // Delete data if any data is allocated
        if (cap) delete[] data;
    }

    // string Literal / C string Constructor
    // REQUIRES: cstr is a null terminated C style string
    // MODIFIES: *this
    // EFFECTS: Creates a string with equivalent contents to cstr
    string(const char *cstr) : sz(0), cap(0), data(nullptr) {
        // Seemingly unneeded initialization but necessary for alloc_

        // Default capacity
        alloc_(DEFAULT_LENGTH);

        if (not cstr) {
            return;
        }


        char *it = data;
        size_t loc = 0;

        while ((*(it++) = *(cstr++))) {
            loc++;

            // Reallocate if capacity is reached
            if (++sz + 1 == cap) {
                alloc_(sz * 2);
                // Reset it as data has moved
                it = data + loc;
            }
        }
    }

    string(const string& other) : sz(0), cap(0), data(nullptr) {
        alloc_(max(other.size()+1, DEFAULT_LENGTH)); //Keep size small unlikely to grow
        for (size_t i = 0; i < other.size(); ++i) {
            data[i] = other[i];
        }
        sz = other.size();
        data[sz] = '\0';
    }


    string& operator=(const string& other) {
        if (this == &other) return *this;

        resize(other.size());

        for (size_t i = 0; i < other.size(); ++i) {
            data[i] = other[i];
        }
        return *this;
    }
    // Size
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns the number of characters in the string
    size_t size() const { return sz; }

    // C string Conversion
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns a pointer to a null terminated C string of *this
    const char *cstr() const { return data; }

    // Iterator Begin
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns a random access iterator to the start of the string
    const char *begin() const { return data; }

    // Iterator End
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns a random access iterator to the end of the string
    const char *end() const { return data + sz; }

    // Element Access
    // REQUIRES: 0 <= i < size()
    // MODIFIES: Allows modification of the i'th element
    // EFFECTS: Returns the i'th character of the string
    char &operator[](size_t i) {
        assert (i < sz);
        return *(data + i);
    }

    char operator[](size_t i) const {
        assert (i < sz);
        return *(data + i);
    }

    // string Append
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Appends the contents of other to *this, resizing any
    //      memory at most once
    void operator+=(const string &other) {
        
        // Need space for all characters, except null terminator
        size_t min_sz = other.size() + sz;

        // Use >= because space is needed for null terminator
        if (min_sz >= cap) {
            alloc_(min_sz + 1);
        }

        // Copy starting from data + sz
        for (size_t i = 0; i < other.size(); ++i) {
            *(data + sz + i) = *(other.begin() + i);
        }

        // Tack on null terminator
        *(data + min_sz) = '\0';
        sz = min_sz;
    }

    // Push Back
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Appends c to the string
    void push_back(char c) {
        if (sz + 1 >= cap) alloc_(max(sz * 2, DEFAULT_LENGTH)); // Allow growth when parsing

        *(data + sz++) = c;
        *(data + sz) = '\0';
    }

    // Pop Back
    // REQUIRES: string is not empty
    // MODIFIES: *this
    // EFFECTS: Removes the last charater of the string
    void pop_back() { *(data + --sz) = '\0'; }

    // Equality Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether all the contents of *this
    //    and other are equal
    bool operator==(const string &other) const {
        if (sz != other.size()) return false;
        for (size_t i = 0; i < sz; ++i) {
            if (*(data + i) != *(other.begin() + i)) return false;
        }
        return true;
    }

    // Not-Equality Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether at least one character differs between
    //    *this and other
    bool operator!=(const string &other) const { return !(*this == other); }

    // Less Than Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether *this is lexigraphically less than other
    bool operator<(const string &other) const {
        size_t min_sz = min(other.size(), sz);
        for (size_t i = 0; i < min_sz; ++i) {
            if (*(data + i) < *(other.begin() + i)) return true;
            if (*(data + i) > *(other.begin() + i)) return false;
        }
        return sz < other.size();
    }

    // Greater Than Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether *this is lexigraphically greater than other
    bool operator>(const string &other) const { return other < *this; }

    // Less Than Or Equal Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether *this is lexigraphically less or equal to other
    bool operator<=(const string &other) const { return !(other < *this); }

    // Greater Than Or Equal Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether *this is lexigraphically GREATER or equal to other
    bool operator>=(const string &other) const { return !(*this < other); }

    bool ends_with(const string &other) const {

        size_t len = other.size();

        if (sz < len) return false;

        for (size_t i = 1; i <= len; ++i) {
            if (data[sz-i] != other[len-i]) return false;
        }
        return true;
    }

    void resize(size_t new_size, char fill_char = '\0') {
        if (new_size >= cap) {
            alloc_(max(new_size+1, DEFAULT_LENGTH)); //Keep size small unlikely to grow
        }
        for (size_t i = sz; i < new_size; ++i) {
            data[i] = fill_char;
        }
        data[new_size] = '\0';
        sz = new_size;
    }


    void shrink_to_fit() {
        if (cap > sz + 1) {
            alloc_(sz + 1);
        }
    }

private:
    size_t sz;
    size_t cap;
    char *data;

    // Allocate space when more size is needed
    void alloc_(size_t new_cap) {
        assert (new_cap >= sz + 1);

        if (new_cap == cap) return;

        char *new_data = new char[new_cap];

        // Iterate to i == sz to include '\0'
        for (size_t i = 0; i < sz; ++i) {
            *(new_data + i) = *(data + i);
        }

        // Tack this back on
        *(new_data + sz) = '\0';

        if (cap) delete[] data;
        data = new_data;
        new_data = nullptr;

        cap = new_cap;
    }
};


std::ostream &operator<<(std::ostream &os, const string &s) {
    for (const char *it = s.begin(); it != s.end(); ++it) {
        os << *it;
    }
    return os;
}


bool stringEq(const string& a, const string& b) {
    return a == b;
}
