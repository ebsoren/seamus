#include "isr.h"
#include "chunk_manager.h"
#include "lib/logger.h"
#include "lib/utf8.h"

#include <cstring>

IndexStreamReader::IndexStreamReader(const string& word, LoadedIndex* index) : word(word.data(), word.size()), index(index) {
    uint64_t offset = index->lookup(this->word);
    if (offset == UINT64_MAX) {
        logger::warn("Word '%s' not found in index", this->word.data());
        n_posts = 0;
        n_docs = 0;
        curr_loc_ = postings_start_ = nullptr;
        return;
    }

    // Offsets are measured from the start of the posting list region (as written
    // by IndexChunk::persist), so no rebasing needed.
    curr_loc_ = postings_start_ = index->posting_list_region_ + offset;

    // Header layout written by IndexChunk::persist:
    //   <8B n_posts><space><4B n_docs><\n>
    memcpy(&n_posts, curr_loc_, sizeof(uint64_t));
    curr_loc_ += sizeof(uint64_t) + 1; // skip over the number and the space

    memcpy(&n_docs, curr_loc_, sizeof(uint32_t));
    curr_loc_ += sizeof(uint32_t) + 1; // skip over the number and the newline
}

const inline post IndexStreamReader::loc() {
    return post{doc_offset_, loc_offset_};
}

post IndexStreamReader::advance_base() {
    if (posts_consumed_ == n_posts) return post{0, 0};

    posts_consumed_++;
    uint32_t loc_or_flag = ReadUtf8(const_cast<const Utf8**>(&curr_loc_), nullptr);
    if (loc_or_flag > 0) {
        loc_offset_ += loc_or_flag;
        return post{doc_offset_, loc_offset_};
    }

    // If we read 0, that was the flag for a new doc
    doc_offset_ += ReadUtf8(&curr_loc_, nullptr); // Add to doc offset
    loc_offset_  = ReadUtf8(&curr_loc_, nullptr); // Reset loc offset
    return post{doc_offset_, loc_offset_};
}

post IndexStreamReader::advance() {
    if (has_pending_) {
        // Overshoot post already in state — serve it without touching curr_loc_.
        // See isr.h comments if confused.
        has_pending_ = false;
        return post{doc_offset_, loc_offset_};
    }
    return advance_base();
}

post IndexStreamReader::advance_to(uint32_t doc) {
    if (has_pending_) {
        // Pending overshoot is at or after `doc`, or it isn't... either way we
        // drop the flag here. If it already satisfies the request, return it.
        // Otherwise fall through to walk forward from the overshoot state.
        has_pending_ = false;
        if (doc_offset_ >= doc) return post{doc_offset_, loc_offset_};
    }
    while (doc_offset_ < doc) {
        post p = advance_base();
        if (p.doc == 0) return p;
    }
    return post{doc_offset_, loc_offset_};
}

// Important note: Because posting list is variable-length, you can't
// just "go back one" after finding the last doc. This is why advance_base,
// has_pending, etc. are necessary.
void IndexStreamReader::collect_positions_in_current_doc(vector<size_t>& out) {
    // Serve the current position (state reflects first post of current doc,
    // or the pending overshoot — either way loc_offset_ is a valid position
    // for doc_offset_).
    uint32_t curr_doc = doc_offset_;
    out.push_back(static_cast<size_t>(loc_offset_));
    has_pending_ = false;

    // Walk forward via raw advance so we can detect a doc change and stash
    // the overshoot without the pending buffer interfering.
    while (true) {
        post p = advance_base();
        if (p.doc == 0) return;   // end of posts
        if (p.doc != curr_doc) {
            has_pending_ = true;
            return;
        }
        out.push_back(static_cast<size_t>(p.loc));
    }
}

string IndexStreamReader::get_url(uint32_t doc) {
    const string_view& sv = index->urls[doc - 1];
    return string(sv.data(), sv.size());
}
