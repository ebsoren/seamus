#include "Index.h"
#include "lib/consts.h"
#include "lib/algorithm.h"
#include "lib/string.h"
#include "lib/utf8.h"
#include "lib/utils.h"
#include "lib/logger.h"
#include <sys/stat.h>

string IndexChunk::get_index_chunk_path() const {
    return string::join("", string(INDEX_OUTPUT_DIR), "/index_chunk_", string(WORKER_NUMBER), "_", string(chunk), ".txt");
}


IndexChunk::IndexChunk(uint32_t worker_number) : curr_doc_(1), chunk(0), docs_in_chunk_(0), posts_bytes_(0), WORKER_NUMBER(worker_number){
    // Important: Init curr_doc_ to 1 to allow for 00 to be used as new doc flag
    // Find the latest chunk ID for this thread
    while (file_exists(get_index_chunk_path())) chunk++;
}


void IndexChunk::persist() {
    mkdir(INDEX_OUTPUT_DIR, 0755); // no-op if already exists
    // Create a file (if it already exists, fail -- don't want to overwrite)
    string path = get_index_chunk_path();
    FILE* fd = fopen(path.data(), "wx");

    if (fd == nullptr) {
        logger::error("Worker %u: failed to open '%s' for writing (errno=%d: %s)", WORKER_NUMBER, path.data(), errno, strerror(errno));
        return;
    }

    // Each entry is: <4B id> <1B space> <url_len bytes> <1B '\n'>
    uint64_t urls_bytes = 0;
    for (size_t i = 0; i < urls.size(); i++) urls_bytes += 4 + 1 + urls[i].size() + 1;

    // Write the size of the ID->URL mapping
    // <64b SIZE>\n
    fwrite(&urls_bytes, sizeof(urls_bytes), 1, fd);
    fwrite("\n", sizeof(char), 1, fd);

    // Write the ID->URL mapping
    // <32b ID> <varlen URL>\n
    for (uint32_t i = 0; i < urls.size(); i++) {
        uint32_t id = i + 1; // 1 indexed
        
        fwrite(&id, sizeof(id), 1, fd);
        fwrite(" ", sizeof(char), 1, fd);
        fwrite(urls[i].data(), sizeof(char), urls[i].size(), fd);
        fwrite("\n", sizeof(char), 1, fd);
    }

    fwrite("\n", sizeof(char), 1, fd);

    vector<string> alphabetized_entries = sort_entries();
    const uint32_t N = alphabetized_entries.size();

    // Each skip entry: <4B doc_id> <1B space> <8B offset> <1B '\n'>
    const uint64_t SKIP_LIST_ENTRY_SIZE = 4 + 1 + 8 + 1;
    const uint64_t SKIP_LIST_SIZE = (DOCS_PER_INDEX_CHUNK / INDEX_SKIP_SIZE) * SKIP_LIST_ENTRY_SIZE;
    // Toggle: when false, skip-list code is kept but does nothing (no bytes reserved, no bytes written).
    constexpr bool WRITE_SKIP_LIST = false;

    /**
     * FIRST PASS over postings
     * Calculate dictionary lookup table values (bytes from start of dict to first word of each letter)
     * Calculate posting list sizes/locations
     * Caclulate skip list sizes (part of posting list sizes) and locations
     *  */

    vector<uint64_t> posting_list_locations(N);
    uint64_t posting_list_size = 0;

    constexpr size_t DICT_SLOTS = 36;
    constexpr uint64_t NO_ENTRY = UINT64_MAX;
    uint64_t dict_offsets[DICT_SLOTS];

    for (size_t i = 0; i < DICT_SLOTS; i++) dict_offsets[i] = NO_ENTRY;
    uint64_t curr_offset = 0;

    for (uint32_t i = 0; i < N; i++) {

        unsigned char first = static_cast<unsigned char>(alphabetized_entries[i][0]);
        size_t slot = (first >= 'a') ? static_cast<size_t>(first - 'a')
                                     : static_cast<size_t>(26 + (first - '0'));
        if (dict_offsets[slot] == NO_ENTRY) {
            dict_offsets[slot] = curr_offset;
        }

        // Dict entry on disk: <word><space><8B offset><\n>. Must match the
        // actual write loop below (fwrite: word + space + uint64_t + '\n').
        curr_offset += alphabetized_entries[i].size() + 1 + sizeof(uint64_t) + 1;

        // Mark where the byte offset where current word's posting list begins
        posting_list_locations[i] = posting_list_size;

        // Header for each posting list: 64 bits each for # posts & # docs, plus 2 separating characters
        posting_list_size += 6 + 6 + 2;

        // Used to calculate the offset (instead of absolute value)
        uint32_t last_doc = 0;
        uint32_t last_loc = 0;

        postings& entry = index[alphabetized_entries[i].str_view(0, alphabetized_entries[i].size())];

        // Mirrors the second-pass writer below: on a new doc, reset last_loc
        // to 0 BEFORE sizing the loc delta, and always advance last_loc to
        // p.loc after. Accounting must match byte-for-byte so posting_list_locations
        // points at the header of each word's posting list.
        for (post p : entry.posts) {
            uint64_t post_size = 0;
            if (p.doc > last_doc) {
                // 1 byte flag + varlen doc delta
                post_size += 1 + SizeOfUtf8(p.doc - last_doc);
                last_doc = p.doc;
                last_loc = 0;
            }
            post_size += SizeOfUtf8(p.loc - last_loc);
            last_loc = p.loc;

            posting_list_size += post_size;
        }

        // Extra 1 for newline at end of each word's posting list
        posting_list_size += (WRITE_SKIP_LIST ? SKIP_LIST_SIZE : 0) + 1;
    }

    // Fill gaps in dict sort order (digits precede letters in the sorted dict).
    // Reverse order is: 'z','y',...,'a', then '9','8',...,'0'. Any empty slot
    // inherits the next-greater slot's offset, so a range read against an
    // empty slot is a legitimate empty range instead of stale memory.
    {
        uint64_t fill = curr_offset;
        for (int i = 25; i >= 0; i--) {
            if (dict_offsets[i] == NO_ENTRY) dict_offsets[i] = fill;
            else fill = dict_offsets[i];
        }
        for (int i = 35; i >= 26; i--) {
            if (dict_offsets[i] == NO_ENTRY) dict_offsets[i] = fill;
            else fill = dict_offsets[i];
        }
    }

    // Write dictionary lookup table
    // <1B CHAR> <64b OFFSET>\n  — slots 0..25 are 'a'..'z', 26..35 are '0'..'9'.
    for (size_t i = 0; i < DICT_SLOTS; i++) {
        char c = (i < 26) ? char(i + 'a') : char((i - 26) + '0');
        fwrite(&c, sizeof(char), 1, fd);
        fwrite(" ", sizeof(char), 1, fd);
        fwrite(dict_offsets + i, sizeof(uint64_t), 1, fd);
        fwrite("\n", sizeof(char), 1, fd);
    }

    fwrite("\n", sizeof(char), 1, fd);

    // Write the dictionary itself
    // <varlen WORD> <64b OFFSET>\n
    for (uint32_t i = 0; i < N; i++) {
        fwrite(alphabetized_entries[i].data(), sizeof(char), alphabetized_entries[i].size(), fd);
        fwrite(" ", sizeof(char), 1, fd);
        fwrite(&posting_list_locations[i], sizeof(uint64_t), 1, fd);
        fwrite("\n", sizeof(char), 1, fd);
    }

    fwrite("\n", sizeof(char), 1, fd);

    /**
     * SECOND PASS over postings
     * Write the posting lists themselves
     */

    // Drift detection: first-pass sizing must match second-pass byte counts
    // exactly, otherwise posting_list_locations[i] points at mid-post-list
    // garbage. At each word header, compare ftell() against
    // post_region_start + posting_list_locations[i]. On mismatch, log the
    // per-word delta (bytes sized vs. bytes written for the PREVIOUS word,
    // which is the word that just caused the new drift). Rate-limited so
    // one bad chunk doesn't flood the log.
    long post_region_start = ftell(fd);
    long prev_word_ftell = post_region_start;
    long prev_cumulative_delta = 0;
    uint32_t drift_events_logged = 0;

    // Loop through all words
    for (uint32_t i = 0; i < N; i++) {
        {
            long expected_offset = post_region_start + static_cast<long>(posting_list_locations[i]);
            long actual_offset = ftell(fd);
            long cumulative_delta = actual_offset - expected_offset;
            if (i > 0 && cumulative_delta != prev_cumulative_delta && drift_events_logged < 20) {
                ++drift_events_logged;
                uint64_t sized_prev_word = posting_list_locations[i] - posting_list_locations[i - 1];
                long wrote_prev_word = actual_offset - prev_word_ftell;
                long per_word_delta = wrote_prev_word - static_cast<long>(sized_prev_word);
                logger::error(
                    "Worker %u: drift at prev word %u '%.*s': sized=%llu wrote=%ld (per-word delta=%ld, cumulative delta=%ld)",
                    WORKER_NUMBER, i - 1,
                    static_cast<int>(alphabetized_entries[i - 1].size()),
                    alphabetized_entries[i - 1].data(),
                    static_cast<unsigned long long>(sized_prev_word),
                    wrote_prev_word,
                    per_word_delta,
                    cumulative_delta);
            }
            prev_word_ftell = actual_offset;
            prev_cumulative_delta = cumulative_delta;
        }
        postings& entry = index[alphabetized_entries[i].str_view(0, alphabetized_entries[i].size())];
        uint64_t size = entry.posts.size(); // Needs to be an lvalue for fwrite
        uint32_t next_checkpoint = INDEX_SKIP_SIZE;

        // Write the number of occurrences and documents
        // <64b NUM POSTS> <32b NUM DOCS>\n
        fwrite(&size, sizeof(uint64_t), 1, fd);
        fwrite(" ", sizeof(char), 1, fd);
        fwrite(&entry.n_docs, sizeof(uint32_t), 1, fd);
        fwrite("\n", sizeof(char), 1, fd);

        // Compute doc offsets on the fly and write skip list
        // For every INDEX_SKIP_SIZE document: <32b DOC ID> <64b BYTE OFFSET FROM START OF SKIP LIST>\n
        // Plus one entry per new doc encountered.
        // The region is padded out to SKIP_LIST_SIZE bytes so the first-pass
        // size accounting in posting_list_locations stays correct.
        if constexpr (WRITE_SKIP_LIST) {
            uint32_t scan_last_doc = 0;
            uint32_t scan_last_loc = 0;
            uint64_t scan_offset = 0;
            uint64_t skip_bytes_written = 0;

            for (size_t pi = 0; pi < entry.posts.size(); ++pi) {
                post p = entry.posts[pi];
                uint64_t post_size = 0;

                if (p.doc > scan_last_doc) {
                    post_size += 1 + SizeOfUtf8(p.doc - scan_last_doc);
                    scan_last_doc = p.doc;
                    scan_last_loc = 0;

                    while (p.doc > next_checkpoint) {
                        // Fill the skip_list with the first posting after <CHECKPOINT> # of documents
                        // If we've passed multiple checkpoints, they'll have the same offset (hence the while loop)
                        uint64_t total_offset = scan_offset + SKIP_LIST_SIZE;
                        fwrite(&next_checkpoint, sizeof(uint32_t), 1, fd);
                        fwrite(" ", sizeof(char), 1, fd);
                        fwrite(&total_offset, sizeof(uint64_t), 1, fd);
                        fwrite("\n", sizeof(char), 1, fd);

                        skip_bytes_written += SKIP_LIST_ENTRY_SIZE;
                        next_checkpoint += INDEX_SKIP_SIZE;
                    }

                    uint64_t total_offset = scan_offset + SKIP_LIST_SIZE;
                    fwrite(&p.doc, sizeof(uint32_t), 1, fd);
                    fwrite(" ", sizeof(char), 1, fd);
                    fwrite(&total_offset, sizeof(uint64_t), 1, fd);
                    fwrite("\n", sizeof(char), 1, fd);

                    skip_bytes_written += SKIP_LIST_ENTRY_SIZE;
                }

                post_size += SizeOfUtf8(p.loc - scan_last_loc);
                scan_last_loc = p.loc;

                scan_offset += post_size;
            }

            // Pad skip list region to SKIP_LIST_SIZE. The per-new-doc writes
            // mean dense inputs can overflow the reservation — flag it loudly
            // if that ever happens (design issue; not fixed here).
            if (skip_bytes_written > SKIP_LIST_SIZE) {
                logger::error("Worker %u: skip list overflow for '%.*s' (%llu > %llu bytes)",
                              WORKER_NUMBER,
                              static_cast<int>(alphabetized_entries[i].size()),
                              alphabetized_entries[i].data(),
                              static_cast<unsigned long long>(skip_bytes_written),
                              static_cast<unsigned long long>(SKIP_LIST_SIZE));
            } else {
                static const char ZERO_BUF[1024] = {0};
                uint64_t pad = SKIP_LIST_SIZE - skip_bytes_written;
                while (pad > 0) {
                    size_t n = pad < sizeof(ZERO_BUF) ? static_cast<size_t>(pad) : sizeof(ZERO_BUF);
                    fwrite(ZERO_BUF, sizeof(char), n, fd);
                    pad -= n;
                }
                logger::debug("Wrote %u bytes of padding for %s.", pad, alphabetized_entries[i].data());
            }
        }

        // Write posts
        uint32_t last_doc = 0;
        uint32_t last_loc = 0;

        Utf8 doc_buff[MAX_UTF8_LEN + 1];
        Utf8 loc_buff[MAX_UTF8_LEN];
        doc_buff[0] = 0; // Set first bit of doc_buff to be the flag

        // For each word occurrence: (IF NEW DOC <0b0||varlen DOC OFFSET>)<varlen LOC ID/OFFSET>
        // Because we're doing UTF 8, no delimiters
        for (post p : entry.posts) {
            // Only write the doc offset if it's a new document, in which case write the offset after the flag
            if (p.doc > last_doc) {
                Utf8* doc_end = WriteUtf8(doc_buff + 1, p.doc - last_doc, doc_buff + MAX_UTF8_LEN + 1);
                fwrite(doc_buff, sizeof(Utf8), doc_end - doc_buff, fd);
                last_loc = 0;
            }

            // Write the loc offset
            Utf8* loc_end = WriteUtf8(loc_buff, p.loc - last_loc, loc_buff + MAX_UTF8_LEN);
            fwrite(loc_buff, sizeof(Utf8), loc_end - loc_buff, fd);

            // Update offsets
            last_loc = p.loc;
            if (p.doc > last_doc) {
                last_doc = p.doc;
            } 
        }

        fwrite("\n", sizeof(char), 1, fd);
    }
    
    fclose(fd);

    chunk++;
}

void IndexChunk::reset() {
    index = unordered_map<string, postings>();
    urls = vector<string>();
    docs_in_chunk_ = 0;
    posts_bytes_ = 0;
    curr_doc_ = 1; // Curr_doc must start at 1, 0 reserved for flag
}


void IndexChunk::flush() {
    persist();
    reset();
    logger::info("Worker %u: flush chunk: %u", WORKER_NUMBER, chunk-1);
}


vector<string> IndexChunk::sort_entries() {
    vector<string> res;
    res.reserve(index.size());

    // Dictionary TOC has 36 slots (a-z + 0-9). Anything else is upstream garbage
    // and would break persist()'s dict_offsets indexing; drop it here and log.
    size_t dropped = 0;
    for (auto it = index.begin(); it != index.end(); ++it) {
        auto& key = (*it).key;
        if (key.size() == 0) { dropped++; continue; }
        unsigned char first = static_cast<unsigned char>(key.data()[0]);
        bool is_letter = (first >= 'a' && first <= 'z');
        bool is_digit  = (first >= '0' && first <= '9');
        if (!is_letter && !is_digit) { dropped++; continue; }
        res.push_back(string(key.data(), key.size()));
    }

    if (dropped > 0) {
        logger::warn("Worker %u: sort_entries dropped %zu/%zu entries with non-[a-z0-9] first byte",
                     WORKER_NUMBER, dropped, index.size());
    }

    radix_sort(res);
    return res;
}

bool IndexChunk::index_file(const string &path) {
    FILE* fd = fopen(path.data(), "r");
    if (fd == nullptr) {
        perror("Error opening file.\n");
        return false;
    }

    char buff[4096];
    char url[2048];

    while (true) {
        // Set of words already encountered in the document to track number of documents word appears in
        unordered_map<string, bool> word_set;

        // Check doc header (or EOF between documents)
        if (!fgets(buff, sizeof(buff), fd)) break;
        if (strcmp(buff, "<doc>\n")) {
            // Corrupt data -- scan forward for the next <doc> header instead of bailing on the file
            logger::warn("Worker %u: expected <doc> header, got: %s in file: %s (scanning for next <doc>)", WORKER_NUMBER, buff, path.data());
            while (fgets(buff, sizeof(buff), fd)) {
                if (!strcmp(buff, "<doc>\n")) break;
            }
            if (feof(fd) || ferror(fd)) break;
        }

        // Read in the URL (will end with \n\0)
        if (!fgets(url, sizeof(url), fd)) break;

        // Defensive: skip doc if URL line is empty or has no content
        size_t url_len = strlen(url);
        if (url_len == 0) {
            logger::warn("Worker %u: empty URL line in file: %s (skipping doc)", WORKER_NUMBER, path.data());
            continue;
        }
        // Strip trailing newline if present
        size_t url_content_len = (url[url_len - 1] == '\n') ? url_len - 1 : url_len;
        if (url_content_len == 0) {
            logger::warn("Worker %u: blank URL in file: %s (skipping doc)", WORKER_NUMBER, path.data());
            continue;
        }

        // Increment the doc count
        uint32_t doc = curr_doc_++;
        urls.push_back(string(url, url_content_len));

        // Start a counter for word locations
        uint32_t loc = 0;

        // Parse title and body words
        while(fgets(buff, sizeof(buff), fd)) {
            if (!strcmp(buff, "</doc>\n")) {
                // Doc ended, go back to outer loop
                break;
            } else if (strcmp(buff, "</title>\n") && strcmp(buff, "<title>\n")) { // Don't push the title tags
                // -1 because all words have new line at the end from fgets
                size_t len = strlen(buff);
                if (len <= 1) continue; // skip empty lines
                string_view word_view = string_view(buff, len - 1);

                if (!word_set[word_view]) {
                    word_set[word_view] = true;
                    index[word_view].n_docs++;
                }

                index[word_view].posts.push_back({doc, ++loc});
                posts_bytes_ += sizeof(post);
            }
        }

        if (ferror(fd)) {
            logger::error("Worker %u: file read error on file: %s, with error: %s", WORKER_NUMBER, path.data(), strerror(errno));
            fclose(fd);
            return false;
        }

        if (++docs_in_chunk_ >= DOCS_PER_INDEX_CHUNK || posts_bytes_ >= CHUNK_MEM_BUDGET) {
            flush();
        }
    }

    fclose(fd);
    logger::info("Worker %u: indexed file: %s", WORKER_NUMBER, path.data());
    return true;
}
