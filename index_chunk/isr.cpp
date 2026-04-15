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
    const uint8_t* region_start = index->posting_list_region_;
    const uint8_t* region_end = index->posting_list_region_end_;
    curr_loc_ = postings_start_ = region_start + offset;

    // Header layout written by IndexChunk::persist:
    //   <8B n_posts><space><4B n_docs><\n>
    if (curr_loc_ + sizeof(uint64_t) + 1 + sizeof(uint32_t) + 1 > region_end) {
        logger::error("ISR '%s': header offset %llu walks past region end (size=%lld)",
                      this->word.data(),
                      static_cast<unsigned long long>(offset),
                      static_cast<long long>(region_end - region_start));
        n_posts = 0;
        n_docs = 0;
        curr_loc_ = postings_start_ = nullptr;
        return;
    }

    memcpy(&n_posts, curr_loc_, sizeof(uint64_t));
    curr_loc_ += sizeof(uint64_t) + 1; // skip over the number and the space

    memcpy(&n_docs, curr_loc_, sizeof(uint32_t));
    curr_loc_ += sizeof(uint32_t) + 1; // skip over the number and the newline

    // Sanity: n_posts can't exceed the number of bytes remaining in the region
    // (minimum encoding is 1 byte per post, so n_posts is bounded by region
    // tail length). Garbage n_posts from a misaligned dict lookup produces
    // absurd values — detect and disable this ISR rather than walk off.
    uint64_t max_plausible_posts = static_cast<uint64_t>(region_end - curr_loc_);
    if (n_posts > max_plausible_posts) {
        // Dump the 16 bytes at the dict-returned offset (i.e. the header the
        // reader expects) so we can see what's actually there on disk.
        const uint8_t* dump = postings_start_;
        logger::error("ISR '%s': n_posts=%llu n_docs=%u exceeds max plausible %llu (offset=%llu, region_tail=%llu)",
                      this->word.data(),
                      static_cast<unsigned long long>(n_posts),
                      static_cast<unsigned>(n_docs),
                      static_cast<unsigned long long>(max_plausible_posts),
                      static_cast<unsigned long long>(offset),
                      static_cast<unsigned long long>(region_end - region_start));
        logger::error("  header bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                      dump[0], dump[1], dump[2], dump[3], dump[4], dump[5], dump[6], dump[7],
                      dump[8], dump[9], dump[10], dump[11], dump[12], dump[13], dump[14], dump[15]);
        n_posts = 0;
        n_docs = 0;
        curr_loc_ = postings_start_ = nullptr;
        return;
    }
}

const inline post IndexStreamReader::loc() {
    return post{doc_offset_, loc_offset_};
}

post IndexStreamReader::advance_base() {
    if (posts_consumed_ == n_posts) return post{0, 0};

    // Defensive walk-off guard: if the decoder is about to read past the end
    // of the posting list region (file EOF), stop. This catches the case
    // where n_posts was read as garbage from a misaligned dict lookup, so
    // posts_consumed_ would never catch up to n_posts naturally.
    if (curr_loc_ >= index->posting_list_region_end_) {
        logger::error("ISR '%s': walk-off (consumed %llu/%llu posts, %u docs)",
                      word.data(),
                      static_cast<unsigned long long>(posts_consumed_),
                      static_cast<unsigned long long>(n_posts),
                      static_cast<unsigned>(doc_offset_));
        posts_consumed_ = n_posts;
        return post{0, 0};
    }

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
