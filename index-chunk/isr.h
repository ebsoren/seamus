#pragma once

#include "lib/string.h"
#include "index/Index.h"

class LoadedIndex;

class IndexStreamReader {
public:
    string word;

    // Useful for heuristics on how common the word is
    // Prefer to satisfy constraints on less common words first
    uint64_t n_posts = 0;
    uint32_t n_docs  = 0;

    IndexStreamReader(const string& word, LoadedIndex* index);

    // Return the current post/location for the given word
    const inline post loc();

    // Advance the ISR to the next post in the index
    // @returns post just read on success; {0, 0} if at last post
    post advance();

    // Advance the ISR to the first post at or after the given document, if one exists
    // If no posts exist for that document, the ISR returns the first post at a document AFTER
    // @returns first post of the doc (or first doc after) on success ; {0, 0} if no posts exist for/after that document
    post advance_to(uint32_t doc);

    string get_url(uint32_t doc);

    const inline void reset() {
        curr_loc_       = postings_start_;
        posts_consumed_ = 0;
        doc_offset_     = 0;
        loc_offset_     = 0;
    }

private:
    // Access & easily traverse the file
    LoadedIndex* index;

    // Pointer to track locations in index postings buffer. The buffer is owned
    // by the LoadedIndex and the ISR only reads from it, so these are const.
    const uint8_t* postings_start_ = nullptr;
    const uint8_t* curr_loc_       = nullptr;

    // Track number of posts we've seen to know whether we're at the end
    uint64_t posts_consumed_ = 0;

    // Track the current deltas
    uint32_t doc_offset_ = 0;
    uint32_t loc_offset_ = 0;
};
