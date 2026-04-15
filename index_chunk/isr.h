#pragma once

#include "index/Index.h"
#include "lib/string.h"
#include "lib/vector.h"

class LoadedIndex;

class IndexStreamReader {
public:
    string word;

    // Useful for heuristics on how common the word is
    // Prefer to satisfy constraints on less common words first
    uint64_t n_posts = 0;
    uint32_t n_docs = 0;

    IndexStreamReader(const string &word, LoadedIndex *index);

    // Return the current post/location for the given word
    const inline post loc();

    // Advance the ISR to the next post in the index
    // @returns post just read on success; {0, 0} if at last post
    post advance();

    // Advance the ISR to the first post at or after the given document, if one exists
    // If no posts exist for that document, the ISR returns the first post at a document AFTER
    // returns {0, 0} if no posts exist for/after that document
    post advance_to(uint32_t doc);

    // Advance past the current doc to the first post of the next doc.
    // Returns {0, 0} if there is no next doc.
    post advance_to_next_doc() { return advance_to(doc_offset_ + 1); }

    // Collect every position of `word` in the current doc into `out`, advancing
    // the ISR past that doc. Precondition: ISR is positioned at the first post
    // of the target doc right after a successful advance_to. The overshoot post 
    // (first post of the next doc) is buffered via has_pending_ so the next 
    // advance()/advance_to() call serves it instead of skipping.
    // Example: [... {doc 1, pos 1}, {doc 1, pos 2}, {doc 2, pos 1}]
    // collect_positions moves until doc != start doc (doc 1), so ends on
    // doc 2. In that case, it's important to set has_pending to true, so
    // doc 2 isn't skipped over on the next advance/advance_to call.
    void collect_positions_in_current_doc(vector<size_t> &out);

    string get_url(uint32_t doc);

    const inline void reset() {
        curr_loc_ = postings_start_;
        posts_consumed_ = 0;
        doc_offset_ = 0;
        loc_offset_ = 0;
        has_pending_ = false;
    }

private:
    // Access & easily traverse the file
    LoadedIndex *index;

    // Pointer to track locations in index postings buffer. The buffer is owned
    // by the LoadedIndex and the ISR only reads from it, so these are const.
    const uint8_t *postings_start_ = nullptr;
    const uint8_t *curr_loc_ = nullptr;

    // Track number of posts we've seen to know whether we're at the end
    uint64_t posts_consumed_ = 0;

    // Track the current deltas
    uint32_t doc_offset_ = 0;
    uint32_t loc_offset_ = 0;

    // When collect_positions_in_current_doc overshoots into the next doc, the
    // overshoot post has already been consumed from curr_loc_ — its values live
    // in doc_offset_/loc_offset_. This flag tells advance()/advance_to() to
    // serve those existing values instead of reading the next post off disk.
    bool has_pending_ = false;

    // Raw advance that skips the pending-buffer check. Used by advance() after
    // it has drained the pending slot, and by collect_positions_in_current_doc
    // which serves the current state directly before walking forward.
    post advance_base();
};
