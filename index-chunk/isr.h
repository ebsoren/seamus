#pragma once

#include "lib/string.h"
#include "index/Index.h"

class LoadedIndex {
private:
    // One contiguous allocation holding the entire chunk file. All other
    // pointers/views below are slices into this buffer — nothing is copied.
    uint8_t* file_buffer_ = nullptr;
    size_t file_size_ = 0;

    struct DictEntry {
        const char* word;         // pointer into file_buffer_
        uint32_t word_len;
        uint64_t posting_offset;  // byte offset from start of posting list region
    };

    // URLs indexed by (doc_id - 1) — doc ids are 1-indexed on disk.
    vector<string_view> urls;

    // Sorted on load (already alphabetized on disk). Binary-searched by lookup().
    vector<DictEntry> dict_entries_;

    // Pointer into file_buffer_, start of the posting list region.
    const uint8_t* posting_list_region_ = nullptr;

    // Binary search dict_entries_ for word. Returns posting-region offset,
    // or UINT64_MAX if not found.
    uint64_t lookup(const string& word) const;

public:
    friend class IndexStreamReader;
    LoadedIndex(const string& path);
    ~LoadedIndex();

    LoadedIndex(const LoadedIndex&)            = delete;
    LoadedIndex& operator=(const LoadedIndex&) = delete;

    // Number of words in the chunk's dictionary, in alphabetized order.
    size_t num_words() const { return dict_entries_.size(); }

    // Construct a string for the word at dict position `i` (alphabetized order).
    string word_at(size_t i) const {
        const DictEntry& e = dict_entries_[i];
        return string(e.word, e.word_len);
    }
};

class IndexStreamReader {
public:
    string word;

    // Useful for heuristics on how common the word is
    // Prefer to satisfy constraints on less common words first
    uint64_t n_posts = 0;
    uint32_t n_docs  = 0;

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
        const string_view& sv = index->urls[doc - 1];
        return string(sv.data(), sv.size());
    }

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
