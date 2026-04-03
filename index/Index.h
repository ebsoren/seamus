#include <mutex>
#include "lib/deque.h"
#include "lib/string.h"
#include "lib/unordered_map.h"
#include "lib/vector.h"

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
    uint32_t chunk;

    size_t posts_count;

    const uint32_t WORKER_NUMBER;

    vector<string> sort_entries();
    void persist();
    void reset();

public:

    IndexChunk(uint32_t worker_number);
    IndexChunk() = delete;

    bool index_file(const string &path);
    void flush();

};

