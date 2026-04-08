#include "lib/string.h"
#include "index/Index.h"

class IndexStreamReader {
public:
    string word;

    // Useful for heuristics on how common the word is
    // Prefer to satisfy constraints on less common words first
    uint32_t n_posts;
    uint32_t n_docs;

    IndexStreamReader(string word, string chunk_path);
private:
    post* curr_loc;

    // Return the current post/location for the given word
    const inline post loc() {
        return *curr_loc;
    }

    // Advance the ISR to the next post in the index
    // @returns next post on success; {0, 0} if at last post
    post advance();

    // Advance the ISR to the first post of a given document, if one exists
    // If no posts exist for that document, the ISR remains at the current location
    // @returns first post of the doc on success; {0, 0} if no posts exist for that document
    post advance_to(uint32_t doc);
};