#include "isr.h"
#include "lib/consts.h"
#include "lib/logger.h"
#include "lib/utf8.h"

#include <cstdio>
#include <cstring>

LoadedIndex::LoadedIndex(const string& path) {
    FILE* fd = fopen(path.data(), "rb");
    if (fd == nullptr) {
        logger::warn("Failed to open %s", path.data());
        return;
    }

    // Load the entire chunk file into a single contiguous buffer.
    // All section pointers below are slices into this buffer.
    fseek(fd, 0, SEEK_END);
    long file_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    if (file_size <= 0) {
        logger::warn("Empty or unseekable file %s", path.data());
        fclose(fd);
        return;
    }
    file_size_ = static_cast<size_t>(file_size);
    file_buffer_ = new uint8_t[file_size_];

    size_t read = fread(file_buffer_, sizeof(uint8_t), file_size_, fd);
    fclose(fd);
    if (read != file_size_) {
        logger::warn("Short read loading %s (%zu/%zu)", path.data(), read, file_size_);
        return;
    }

    // --- Parse sections by walking pointers into file_buffer_ ---
    //
    // On-disk layout (see IndexChunk::persist):
    //   <8B urls_bytes><\n>
    //   <4B id><space><url><\n> × N_docs
    //   <\n>
    //   <1B char><space><8B offset><\n> × 36  (dictionary TOC)
    //   <\n>
    //   <word><space><8B offset><\n> × N_words  (dictionary)
    //   <\n>
    //   <posting lists>

    const uint8_t* p = file_buffer_;
    const uint8_t* end = file_buffer_ + file_size_;

    // 1. URL table size header
    uint64_t urls_bytes;
    memcpy(&urls_bytes, p, sizeof(uint64_t));
    p += sizeof(uint64_t);
    if (p < end && *p == '\n') p++; // persist writes a '\n' after the size header

    // 2. URL table: <4B id><space><url><\n>
    //    Doc ids are written sequentially (1-indexed) in insertion order, so
    //    push_back preserves order and urls[doc - 1] is the URL for doc.
    urls.reserve(DOCS_PER_INDEX_CHUNK);
    const uint8_t* urls_end = p + urls_bytes;
    if (urls_end > end) {
        logger::warn("URL table overruns file in %s", path.data());
        return;
    }
    while (p < urls_end) {
        // Skip 4B id + 1B space. Ids are sequential, so we don't need to store them.
        p += sizeof(uint32_t) + 1;
        const uint8_t* nl = static_cast<const uint8_t*>(memchr(p, '\n', urls_end - p));
        if (!nl) {
            logger::warn("Malformed URL line in %s", path.data());
            return;
        }
        urls.push_back(string_view(reinterpret_cast<const char*>(p), nl - p));
        p = nl + 1;
    }

    // Blank line separator after URL table
    if (p < end && *p == '\n') p++;

    // 3. Dictionary TOC — fixed size, skip past for now. The TOC is still
    //    useful as a first-letter filter to narrow binary search; wire that
    //    up as a future optimization.
    p += INDEX_DICTIONARY_TOC_SIZE;
    if (p < end && *p == '\n') p++; // blank line separator after TOC

    // 4. Dictionary entries: <word><space><8B offset><\n>, terminated by a bare '\n'.
    //    Words are restricted to [a-z0-9] (see IndexChunk::sort_entries), so the
    //    first space byte reliably marks end-of-word even though the 8B offset
    //    that follows may contain 0x20/0x0A bytes.
    while (p < end) {
        if (*p == '\n') { p++; break; } // terminator

        const uint8_t* space = static_cast<const uint8_t*>(memchr(p, ' ', end - p));
        if (!space) {
            logger::warn("Malformed dictionary entry in %s", path.data());
            return;
        }

        DictEntry entry;
        entry.word = reinterpret_cast<const char*>(p);
        entry.word_len = static_cast<uint32_t>(space - p);
        memcpy(&entry.posting_offset, space + 1, sizeof(uint64_t));
        dict_entries_.push_back(entry);

        // Advance past: word + space + 8B offset + '\n'
        p = space + 1 + sizeof(uint64_t) + 1;
    }

    // 5. Posting list region starts here
    posting_list_region_ = p;
}

LoadedIndex::~LoadedIndex() {
    delete[] file_buffer_;
}

uint64_t LoadedIndex::lookup(const string& word) const {
    const char* target = word.data();
    size_t target_len = word.size();

    size_t lo = 0;
    size_t hi = dict_entries_.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const DictEntry& e = dict_entries_[mid];
        size_t min_len = target_len < e.word_len ? target_len : e.word_len;
        int cmp = memcmp(e.word, target, min_len);
        if (cmp == 0) {
            if (e.word_len == target_len) return e.posting_offset;
            cmp = (e.word_len < target_len) ? -1 : 1;
        }
        if (cmp < 0) lo = mid + 1;
        else         hi = mid;
    }
    return UINT64_MAX;
}

IndexStreamReader::IndexStreamReader(string word, LoadedIndex* index) : word(move(word)), index(index) {
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
    uint32_t loc_or_flag = ReadUtf8(&curr_loc_, nullptr);
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
