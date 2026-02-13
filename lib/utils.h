//
// Created by Aiden Mizhen on 2/10/26.
//
#pragma once

#include <sys/stat.h>
#include "string.h"


void stem_word(string& word) {
    bool double_check = false;
    if (word.ends_with("ing") and word.size() >= 6) {
        word.resize(word.size() - 3);
        double_check = true;
    }
    else if (word.ends_with("ed") and word.size() >= 5) {
        word.resize(word.size() - 2);
        double_check = true;
    }
    else if (word.ends_with("ly") and word.size() >= 5) {
        word.resize(word.size() - 2);
    }
    else if (word.ends_with("ful") and word.size() >= 6) {
        word.resize(word.size() - 3);
    }
    else if (word.ends_with("ness") and word.size() >= 7) {
        word.resize(word.size() - 4);
    }
    else if (word.ends_with("es") and word.size() >= 5) {
        word.resize(word.size() - 2);
    }
    else if (word.ends_with("s") and word.size() >= 4) {
        word.resize(word.size() - 1);
    }
    else if (word.ends_with("er") and word.size() >= 7) {
        word.resize(word.size() - 2);
    }
    if (double_check) {
        if (word[word.size()-1] == word[word.size()-2] and
            word[word.size()-1] != 'l' and word[word.size()-1] != 's' and word[word.size()-1] != 'z') {
            word.resize(word.size() - 1);
            }
    }
}

double double_pow(double base, int exp) {
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

bool file_exists(const string &fname) {
    struct stat buff;
    return stat(fname.cstr(), &buff) == 0;
}