#pragma once

#include "string.h"
#include "vector.h"

// Important structs for query return type

struct WordInfo {
    string word;
    vector<size_t> pos;
};

struct DocInfo {
    string url;
    vector<WordInfo> wordInfo;
};

struct QueryResponse {
    vector<DocInfo> pages;
};
