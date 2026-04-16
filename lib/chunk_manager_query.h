#pragma once

#include "string.h"
#include "vector.h"

// Important structs for query logic

struct NodeInfo {
    string phrase;
    vector<size_t> pos; // word_cnt can be calculated from the size of this
    size_t freqs;
    bool is_phrase;
};

struct DocInfo {
    string url;
    vector<NodeInfo> nodeInfo;
};

struct ChunkQueryInfo {
    vector<DocInfo> pages;
};