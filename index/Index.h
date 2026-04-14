#pragma once

#include <mutex>
#include "lib/deque.h"
#include "lib/string.h"
#include "lib/unordered_map.h"
#include "lib/vector.h"
#include "../index-stream-reader/isr.h"

// Note that both documents and locations are 1-indexed (so that 0 can be used as a flag)

struct post {
    uint32_t doc;
    uint32_t loc;
};

struct postings {
    vector<post> posts;
    uint64_t n_docs;
};

class IndexChunk {
private:
    unordered_map<string, postings> index;
    vector<string> urls;

    uint32_t curr_doc_;
    uint32_t chunk;

    size_t doc_count;

    const uint32_t WORKER_NUMBER;

    vector<string> sort_entries();
    void persist();
    void reset();
    string get_index_chunk_path() const;

public:

    IndexChunk(uint32_t worker_number);
    IndexChunk() = delete;

    bool index_file(const string &path);
    void flush();

};

class LoadedIndex;

void recover_index_chunks(vector<LoadedIndex>& index_chunks);

