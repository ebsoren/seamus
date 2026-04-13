#include "lib/string.h"
#include "index/Index.h"

class LoadedIndex {
private:
    vector<string> urls;
    unordered_map<string, uint64_t> dictionary;
    uint8_t* posting_list_;
    uint64_t dictionary_size = 0;

public:
    friend class IndexStreamReader;
    LoadedIndex(string path);
    ~LoadedIndex();
};

class IndexStreamReader {
public:
    string word;

    // Useful for heuristics on how common the word is
    // Prefer to satisfy constraints on less common words first
    uint64_t n_posts;
    uint64_t n_docs;

    IndexStreamReader(string word, LoadedIndex* index);

    // Return the current post/location for the given word
    const inline post loc();

    // Advance the ISR to the next post in the index
    // @returns post just read on success; {0, 0} if at last post
    post advance();

    // Advance the ISR to the first post at or after the given document, if one exists
    // If no posts exist for that document, the ISR returns the first post at a document AFTER
    // @returns first post of the doc (or first doc after) on success ; {0, 0} if no posts exist for/after that document
    post advance_to(uint32_t doc);

    const inline string get_url(uint32_t doc) {
        return string(index->urls[doc].data());
    }

    const inline void reset() {
        curr_loc_ = postings_start_;
    }

private:
    // Access & easily traverse the file
    LoadedIndex* index;

    // Pointer to track locations in index postings buffer
    uint8_t* postings_start_;
    uint8_t* curr_loc_;

    // Track number of posts we've seen to know whether we're at the end
    uint64_t posts_consumed_ = 0;
    
    // Track the current deltas
    uint32_t doc_offset_ = 0;
    uint32_t loc_offset_ = 0;
};
