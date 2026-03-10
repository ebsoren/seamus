#include "lib/vector.h"
#include "lib/string.h"
#include "lib/unordered_map.h"

// Note that both documents and locations are 1-indexed (so that 0 can be used as a flag)

struct post {
    uint32_t doc;
    uint32_t loc;
};

struct postings {
    vector<post> posts;
    uint32_t n_docs;
};

class IndexChunk {
private:
    unordered_map<string, postings> index;
    vector<string> urls;
    uint32_t curr_doc_;

    vector<string> sort_entries();
public:
    IndexChunk() : curr_doc_ (1) {}

    bool add_page(const string &path);
    void persist();
};

uint32_t chunk = 0;

// TODO: This needs to be defined globally on bootup (defining here for now)
const uint32_t WORKER_NUMBER = 0;

IndexChunk init_index();
