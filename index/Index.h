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
    std::mutex doc_lock_;
    uint32_t curr_doc_;

    vector<string> sort_entries();
public:
    IndexChunk() : curr_doc_ (1) {}

    bool index_file(const string &path);
    void persist();
};

inline deque<string> files;
inline std::mutex file_lock;
inline uint32_t WORKER_NUMBER;
inline uint32_t chunk = 0;
void init_index();
