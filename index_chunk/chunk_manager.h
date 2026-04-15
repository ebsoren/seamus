#pragma once

#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "isr.h"
#include "index/Index.h"
#include "lib/algorithm.h"
#include "lib/consts.h"
#include "lib/logger.h"
#include "lib/string.h"
#include "lib/utils.h"
#include "lib/vector.h"


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
    uint64_t lookup(const string& word) const {
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

public:
    friend class IndexStreamReader;

    explicit LoadedIndex(const string& path) {
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

    ~LoadedIndex() {
        delete[] file_buffer_;
    }

    LoadedIndex(const LoadedIndex&)            = delete;
    LoadedIndex& operator=(const LoadedIndex&) = delete;

    // Move ctor — required for vector<LoadedIndex> realloc, since LoadedIndex
    // owns file_buffer_ and can't be copied. Null out the source so its
    // destructor doesn't double-free.
    LoadedIndex(LoadedIndex&& other) noexcept
        : file_buffer_(other.file_buffer_),
          file_size_(other.file_size_),
          urls(move(other.urls)),
          dict_entries_(move(other.dict_entries_)),
          posting_list_region_(other.posting_list_region_) {
        other.file_buffer_ = nullptr;
        other.file_size_ = 0;
        other.posting_list_region_ = nullptr;
    }

    // Number of words in the chunk's dictionary, in alphabetized order.
    size_t num_words() const { return dict_entries_.size(); }

    // Construct a string for the word at dict position `i` (alphabetized order).
    string word_at(size_t i) const {
        const DictEntry& e = dict_entries_[i];
        return string(e.word, e.word_len);
    }
};


// Loads every chunk file in INDEX_OUTPUT_DIR into memory. Caller owns the vector.
// Files are named <INDEX_OUTPUT_DIR>/index_chunk_<worker>_<chunk>.txt — see
// IndexChunk::get_index_chunk_path. For each worker slot, walk chunks starting
// at 0 until the first missing file.
inline void recover_index_chunks(vector<LoadedIndex>& index_chunks) {
    for (uint32_t w = 0; w < NUM_INDEXER_THREADS; ++w) {
        for (uint32_t c = 0; ; ++c) {
            string fileName = string::join("",
                                           string(INDEX_OUTPUT_DIR),
                                           "/index_chunk_",
                                           string(w),
                                           "_",
                                           string(c),
                                           ".txt");
            if (!file_exists(fileName)) break;
            index_chunks.emplace_back(fileName);
        }
    }
}


class chunk_manager {
public:
    // Per-call wall-clock budget for get_docIDs, in ms. Defaults to the
    // consts.h value; tests can reassign it to relax or tighten the bound.
    static inline uint32_t deadline_ms = CHUNK_MANAGER_DEADLINE_MS;

    explicit chunk_manager(const string& path) : li(path) {}

    chunk_manager(const chunk_manager&)            = delete;
    chunk_manager& operator=(const chunk_manager&) = delete;

    // Return all doc ids in this chunk where every query word appears.
    // Uses a leapfrog join driven by the rarest word. If the loop runs past
    // `deadline_ms`, returns the matches found so far (a partial result) so
    // the caller isn't starved by a single slow chunk.
    // INVARIANT: WORDS VECTOR MUST CONTAIN UNIQUE ELEMENTS
    vector<uint32_t> default_query(const vector<string>& words) {
        using clock = std::chrono::steady_clock;
        const auto start_time = clock::now();

        vector<uint32_t> doc_ids;
        if (words.size() == 0) return doc_ids;

        vector<IndexStreamReader> isrs;
        isrs.reserve(words.size());
        for (size_t i = 0; i < words.size(); ++i) {
            isrs.push_back(IndexStreamReader(words[i], &li));
        }

        // If any word has zero posts, the AND intersection is empty.
        for (size_t i = 0; i < isrs.size(); ++i) {
            if (isrs[i].n_posts == 0) return doc_ids;
        }

        // Drive leapfrog from the rarest word (fewest documents).
        sort(isrs, compare_by_n_docs);

        post p = isrs[0].advance();
        if (p.doc == 0) return doc_ids;
        uint32_t target = p.doc;

        while (true) {
            // Timer check
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                clock::now() - start_time).count();
            if (static_cast<uint32_t>(elapsed) >= deadline_ms) {
                logger::warn("chunk_manager::get_docIDs hit %ums deadline after %zu matches",
                             deadline_ms, doc_ids.size());
                return doc_ids;
            }

            bool all_match = true;
            for (size_t i = 1; i < isrs.size(); ++i) {
                post q = isrs[i].advance_to(target);
                if (q.doc == 0) return doc_ids;
                if (q.doc > target) {
                    target = q.doc;
                    all_match = false;
                    break;
                }
            }

            if (!all_match) {
                // Pull the driver up to the new target and keep leaping.
                post q = isrs[0].advance_to(target);
                if (q.doc == 0) return doc_ids;
                if (q.doc > target) target = q.doc;
                continue;
            }

            // All words match at `target` — emit and step past it.
            doc_ids.push_back(target);
            p = isrs[0].advance_to(target + 1);
            if (p.doc == 0) return doc_ids;
            target = p.doc;
        }
    }

private:
    LoadedIndex li;

    static bool compare_by_n_docs(const IndexStreamReader& a, const IndexStreamReader& b) {
        return a.n_docs < b.n_docs;
    }
};
