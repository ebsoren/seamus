#include "lib/vector.h"
#include "lib/string.h"
#include "lib/unordered_map.h"

struct post {
    uint32_t doc;
    uint32_t loc;
};

struct postings {
    vector<post> posts;
    uint32_t n_docs;
};

class IndexChunk {
    unordered_map<string, postings> index;
    unordered_map<uint32_t, string> urls;
    uint32_t curr_doc;
    uint32_t curr_loc;

    IndexChunk() : curr_doc (0), curr_loc (0) {}

    // All TODO -- just laying out the general fn list
    void add_page();
    void persist();
};

uint32_t chunk = 0;

void init_index();