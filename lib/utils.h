//
// Created by Aiden Mizhen on 2/10/26.
//
#pragma once

#include <sys/stat.h>
#include "string.h"
#include "consts.h"


inline string_view stem_word(string_view word) {
    bool double_check = false;
    if (word.ends_with("ing") and word.size() >= 6) {
        word = word.substr(0, word.size() - 3);
        double_check = true;
    }
    else if (word.ends_with("ed") and word.size() >= 5) {
        word = word.substr(0, word.size() - 2);
        double_check = true;
    }
    else if (word.ends_with("ly") and word.size() >= 5) {
        word = word.substr(0, word.size() - 2);
    }
    else if (word.ends_with("ful") and word.size() >= 6) {
        word = word.substr(0, word.size() - 3);
    }
    else if (word.ends_with("ness") and word.size() >= 7) {
        word = word.substr(0, word.size() - 4);
    }
    else if (word.ends_with("es") and word.size() >= 5) {
        word = word.substr(0, word.size() - 2);
    }
    else if (word.ends_with("s") and word.size() >= 4) {
        word = word.substr(0, word.size() - 1);
    }
    else if (word.ends_with("er") and word.size() >= 7) {
        word = word.substr(0, word.size() - 2);
    }
    if (double_check) {
        if (word[word.size()-1] == word[word.size()-2] and
            word[word.size()-1] != 'l' and word[word.size()-1] != 's' and word[word.size()-1] != 'z') {
            word = word.substr(0, word.size() - 1);
            }
    }
    return word;
}

inline double double_pow(double base, int exp) {
    double result = 1.0;

    bool neg = (exp < 0);
    if (neg) exp = -exp;

    while (exp > 0)
    {
        if (exp & 1)
            result *= base;

        base *= base;
        exp >>= 1;
    }

    if(neg == true) {
        return 1.0 / result;
    } else {
        return result;
    }
}

inline bool file_exists(const string &fname) {
    struct stat buff;
    return stat(fname.data(), &buff) == 0;
}

// Returns null terminator if index is past string bounds
// Oterhwise returns char at given index in string
inline char charAt(const string &str, size_t i) {
    return i >= str.size() ? '\0' : str[i];
}

// Extract domain/host

// Extracts just the domain from a URL
// e.g. "https://www.example.com/path/page" -> "example.com"
inline string extract_domain(const string& url) {
    const char* p = url.data();
    size_t len = url.size();

    // Strip http:// or https://
    if (len >= 8 && memcmp(p, "https://", 8) == 0) {
        p += 8; len -= 8;
    } else if (len >= 7 && memcmp(p, "http://", 7) == 0) {
        p += 7; len -= 7;
    }

    // Strip www.
    if (len >= 4 && memcmp(p, "www.", 4) == 0) {
        p += 4; len -= 4;
    }

    // Find end of domain (first '/' or end of string)
    size_t domain_len = 0;
    while (domain_len < len && p[domain_len] != '/') {
        domain_len++;
    }

    return string(p, domain_len);
}

inline string extract_host(const string& url) {
    const char* p = url.data();
    size_t len = url.size();

    // Strip scheme
    if (len >= 8 && memcmp(p, "https://", 8) == 0) {
        p += 8; len -= 8;
    } else if (len >= 7 && memcmp(p, "http://", 7) == 0) {
        p += 7; len -= 7;
    }

    // Find end of host (first '/' or end of string)
    size_t host_len = 0;
    while (host_len < len && p[host_len] != '/') {
        host_len++;
    }

    return string(p, host_len);
}

// Returns the index into MACHINES[] for the machine responsible for a given URL's domain
inline size_t get_destination_machine_from_url(const string& url) {
    size_t hash = 0xcbf29ce484222325ULL;
    size_t fnv_prime = 0x100000001b3ULL;

    for (size_t i = 0; i < url.size(); ++i) {
        hash ^= static_cast<unsigned char>(url[i]);
        hash *= fnv_prime;
    }

    return hash % NUM_MACHINES;
}

// Move and swap utilities
template <typename T>
struct remove_reference {typedef T type;};

template <typename T>
struct remove_reference<T&> {typedef T type;};

template <typename T> 
struct remove_reference<T&&> {typedef T type;};

template<typename T>
constexpr inline typename remove_reference<T>::type&& move(T &&t) {
    return static_cast<typename remove_reference<T>::type &&>(t);
}

template <typename T>
void swap(T &lhs, T &rhs) noexcept {
    T tmp(move(rhs));
    rhs = move(lhs);
    lhs = move(tmp);
}
