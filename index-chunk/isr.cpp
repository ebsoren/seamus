#include "isr.h"
#include "chunk-manager.h"
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

post IndexStreamReader::advance() {
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

post IndexStreamReader::advance_to(uint32_t doc) {
    while (doc_offset_ < doc) {
        post p = advance();
        if (p.doc == 0) return p;
    }
    return post{doc_offset_, loc_offset_};
}

string IndexStreamReader::get_url(uint32_t doc) {
    const string_view& sv = index->urls[doc - 1];
    return string(sv.data(), sv.size());
}
