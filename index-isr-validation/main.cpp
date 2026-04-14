#include "index/Index.h"
#include "index-stream-reader/isr.h"
#include "lib/consts.h"
#include "lib/logger.h"
#include "lib/string.h"
#include "lib/utf8.h"
#include "lib/vector.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <random>
#include <sys/stat.h>
#include <unistd.h>


// ---- Benchmark configuration ----------------------------------------------

namespace bench {

constexpr size_t NUM_FILES       = 10;
constexpr size_t DOCS_PER_FILE   = 10;
constexpr size_t WORDS_PER_DOC   = 1000;

// Size of the per-letter word pool we draw document words from.
constexpr size_t WORDS_PER_LETTER = 100;
constexpr size_t MIN_WORD_LEN     = 3;
constexpr size_t MAX_WORD_LEN     = 10;

// Fixed seed for the word pool: we always want the same 36 buckets of
// words so the dictionary under test is stable across runs.
constexpr uint64_t POOL_RNG_SEED = 0xBEEFCAFEULL;

// Where to write the generated parser-format doc files.
constexpr const char* BENCH_DOC_DIR = "/tmp/index_isr_validation";

// How much detail to print when dumping the index chunk.
constexpr size_t DUMP_DICT_WORDS      = 20; // first N dict entries
constexpr size_t DUMP_POSTING_LISTS   = 3;  // first N posting lists (full detail)
constexpr size_t DUMP_SKIPS_PER_LIST  = 10; // first N skip entries per posting list
constexpr size_t DUMP_POSTS_PER_LIST  = 15; // first N posts per posting list

} // namespace bench


// ---- Word pool -------------------------------------------------------------
//
// A single flat vector of words. Entries are grouped by starting character:
// [0 .. WORDS_PER_LETTER)          start with 'a'
// [WORDS_PER_LETTER .. 2*WPL)      start with 'b'
// ...
// [25*WPL .. 26*WPL)               start with 'z'
// [26*WPL .. 27*WPL)               start with '0'
// ...
// [35*WPL .. 36*WPL)               start with '9'
// First char is the bucket character; remaining chars are lowercase [a-z].

struct WordPool {
    vector<string> words;
};


static WordPool build_word_pool(std::mt19937_64& rng) {
    WordPool pool;
    pool.words.reserve(36 * bench::WORDS_PER_LETTER);

    std::uniform_int_distribution<size_t> len_dist(bench::MIN_WORD_LEN, bench::MAX_WORD_LEN);
    std::uniform_int_distribution<int>    tail_dist('a', 'z');

    char buf[bench::MAX_WORD_LEN];

    for (int letter = 0; letter < 26; ++letter) {
        for (size_t i = 0; i < bench::WORDS_PER_LETTER; ++i) {
            size_t len = len_dist(rng);
            buf[0] = static_cast<char>('a' + letter);
            for (size_t k = 1; k < len; ++k) {
                buf[k] = static_cast<char>(tail_dist(rng));
            }
            pool.words.push_back(string(buf, len));
        }
    }

    for (int digit = 0; digit < 10; ++digit) {
        for (size_t i = 0; i < bench::WORDS_PER_LETTER; ++i) {
            size_t len = len_dist(rng);
            buf[0] = static_cast<char>('0' + digit);
            for (size_t k = 1; k < len; ++k) {
                buf[k] = static_cast<char>(tail_dist(rng));
            }
            pool.words.push_back(string(buf, len));
        }
    }
    return pool;
}


static void log_word_pool(const WordPool& pool) {
    char line[4096];
    for (int letter = 0; letter < 26; ++letter) {
        size_t pos = 0;
        line[pos++] = static_cast<char>('a' + letter);
        line[pos++] = ':';
        size_t start = letter * bench::WORDS_PER_LETTER;
        size_t end   = start + bench::WORDS_PER_LETTER;
        for (size_t i = start; i < end && pos + pool.words[i].size() + 2 < sizeof(line); ++i) {
            line[pos++] = ' ';
            memcpy(line + pos, pool.words[i].data(), pool.words[i].size());
            pos += pool.words[i].size();
        }
        line[pos] = '\0';
        logger::instr("%s", line);
    }
    for (int digit = 0; digit < 10; ++digit) {
        size_t pos = 0;
        line[pos++] = static_cast<char>('0' + digit);
        line[pos++] = ':';
        size_t start = (26 + digit) * bench::WORDS_PER_LETTER;
        size_t end   = start + bench::WORDS_PER_LETTER;
        for (size_t i = start; i < end && pos + pool.words[i].size() + 2 < sizeof(line); ++i) {
            line[pos++] = ' ';
            memcpy(line + pos, pool.words[i].data(), pool.words[i].size());
            pos += pool.words[i].size();
        }
        line[pos] = '\0';
        logger::instr("%s", line);
    }
}


// ---- Corpus ----------------------------------------------------------------
//
// A Corpus holds the ground-truth sequence of words for every generated
// document. Layout:
//
//   urls[g]      = URL for global doc g (g = file*DOCS_PER_FILE + doc_in_file)
//   doc_words[g] = sequence of pool indices (one per word position) for doc g
//
// The order inside each doc_words[g] is exactly the order we write to the
// parser-format file, which is also the order IndexChunk will observe during
// indexing. Later, we can rebuild the expected doc from the index and diff
// against this.

struct Corpus {
    vector<string> urls;
    vector<vector<uint32_t>> doc_words;
};


static Corpus generate_corpus(const WordPool& pool, std::mt19937_64& rng) {
    Corpus c;
    const size_t total_docs = bench::NUM_FILES * bench::DOCS_PER_FILE;
    c.urls.reserve(total_docs);
    c.doc_words.reserve(total_docs);

    std::uniform_int_distribution<uint32_t> word_dist(0, pool.words.size() - 1);

    char url_buf[64];
    for (size_t f = 0; f < bench::NUM_FILES; ++f) {
        for (size_t d = 0; d < bench::DOCS_PER_FILE; ++d) {
            int n = snprintf(url_buf, sizeof(url_buf), "http://bench.local/f%zu/d%zu", f, d);
            c.urls.push_back(string(url_buf, static_cast<size_t>(n)));

            vector<uint32_t> words;
            words.reserve(bench::WORDS_PER_DOC);
            for (size_t w = 0; w < bench::WORDS_PER_DOC; ++w) {
                words.push_back(word_dist(rng));
            }
            c.doc_words.push_back(static_cast<vector<uint32_t>&&>(words));
        }
    }
    return c;
}


// ---- File writing ----------------------------------------------------------
//
// Parser output format, per IndexChunk::index_file():
//
//   <doc>\n
//   <url>\n
//   word1\n
//   word2\n
//   ...
//   </doc>\n

static string doc_file_path(size_t file_idx) {
    return string::join("",
                        string(bench::BENCH_DOC_DIR),
                        "/docs_",
                        string(static_cast<uint32_t>(file_idx)),
                        ".txt");
}


// Remove all regular files inside `dir` and then the directory itself.
// Non-recursive: assumes the bench dir only ever contains flat files.
static void cleanup_dir(const char* dir) {
    DIR* d = opendir(dir);
    if (d == nullptr) {
        if (errno != ENOENT) {
            logger::error("opendir %s failed (errno=%d: %s)", dir, errno, strerror(errno));
        }
        return;
    }

    char path[1024];
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (unlink(path) != 0) {
            logger::error("unlink %s failed (errno=%d: %s)", path, errno, strerror(errno));
        }
    }
    closedir(d);

    if (rmdir(dir) != 0 && errno != ENOENT) {
        logger::error("rmdir %s failed (errno=%d: %s)", dir, errno, strerror(errno));
    }
}


// mkdir -p equivalent: create each path component in turn, ignoring EEXIST.
static void mkdir_p(const char* path) {
    char buf[512];
    size_t n = strlen(path);
    if (n >= sizeof(buf)) return;
    memcpy(buf, path, n + 1);
    for (size_t i = 1; i < n; ++i) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
                logger::error("mkdir %s failed (errno=%d: %s)", buf, errno, strerror(errno));
            }
            buf[i] = '/';
        }
    }
    if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
        logger::error("mkdir %s failed (errno=%d: %s)", buf, errno, strerror(errno));
    }
}


static void write_corpus_files(const WordPool& pool, const Corpus& c) {
    mkdir_p(bench::BENCH_DOC_DIR);

    for (size_t f = 0; f < bench::NUM_FILES; ++f) {
        string path = doc_file_path(f);
        FILE* fd = fopen(path.data(), "w");
        if (fd == nullptr) {
            logger::error("failed to open %s for writing (errno=%d: %s)",
                          path.data(), errno, strerror(errno));
            return;
        }

        for (size_t d = 0; d < bench::DOCS_PER_FILE; ++d) {
            size_t g = f * bench::DOCS_PER_FILE + d;

            fputs("<doc>\n", fd);
            fwrite(c.urls[g].data(), 1, c.urls[g].size(), fd);
            fputc('\n', fd);

            const vector<uint32_t>& words = c.doc_words[g];
            for (size_t w = 0; w < words.size(); ++w) {
                const string& word = pool.words[words[w]];
                fwrite(word.data(), 1, word.size(), fd);
                fputc('\n', fd);
            }

            fputs("</doc>\n", fd);
        }

        fclose(fd);
        logger::instr("wrote %s (%zu docs)", path.data(), bench::DOCS_PER_FILE);
    }
}


// ---- Indexing worker -------------------------------------------------------
//
// Mirrors index/main.cpp::worker but:
//   - runs on a single thread (worker 0)
//   - iterates the benchmark's doc files directly
//   - always calls flush() at the end (our NUM_FILES * DOCS_PER_FILE docs never hit the
//     DOCS_PER_INDEX_CHUNK=500k auto-flush threshold)
//   - times the in-memory indexing and the persist-to-disk phases separately

struct BenchTimings {
    double index_ms;
    double flush_ms;
};


static BenchTimings run_index_worker() {
    using clock = std::chrono::steady_clock;

    IndexChunk idx(0);

    auto t0 = clock::now();
    for (size_t f = 0; f < bench::NUM_FILES; ++f) {
        string path = doc_file_path(f);
        if (!idx.index_file(path)) {
            logger::error("index_file failed for %s", path.data());
        }
    }
    auto t1 = clock::now();
    idx.flush();
    auto t2 = clock::now();

    BenchTimings out;
    out.index_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    out.flush_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    return out;
}


// Remove stale worker-0 chunk files so IndexChunk's constructor starts at
// chunk 0 and persist() doesn't hit `wx` EEXIST.
static void cleanup_worker0_chunks() {
    DIR* d = opendir(INDEX_OUTPUT_DIR);
    if (d == nullptr) {
        if (errno != ENOENT) {
            logger::error("opendir %s failed (errno=%d: %s)", INDEX_OUTPUT_DIR, errno, strerror(errno));
        }
        return;
    }

    const char* prefix = "index_chunk_0_";
    const size_t prefix_len = strlen(prefix);
    char path[1024];

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (strncmp(entry->d_name, prefix, prefix_len) != 0) continue;
        snprintf(path, sizeof(path), "%s/%s", INDEX_OUTPUT_DIR, entry->d_name);
        if (unlink(path) != 0) {
            logger::error("unlink %s failed (errno=%d: %s)", path, errno, strerror(errno));
        }
    }
    closedir(d);
}


// ---- Index chunk reader ----------------------------------------------------
//
// Prints the on-disk layout produced by IndexChunk::persist, section by section

static string index_chunk_path(uint32_t worker, uint32_t chunk) {
    return string::join("",
                        string(INDEX_OUTPUT_DIR),
                        "/index_chunk_",
                        string(worker),
                        "_",
                        string(chunk),
                        ".txt");
}


// Read one UTF-8 codepoint from a FILE*. Returns 0 on EOF.
static Unicode read_utf8_from_file(FILE* fd) {
    Utf8 buf[MAX_UTF8_LEN];
    if (fread(buf, 1, 1, fd) != 1) return 0;
    size_t n = IndicatedLength(buf);
    if (n > 1) {
        if (fread(buf + 1, 1, n - 1, fd) != n - 1) return 0;
    }
    const Utf8* p = buf;
    return ReadUtf8(&p, buf + n);
}


struct DictEntry {
    string    word;
    uint64_t  posting_offset;
};


static void dump_index_chunk(const string& path) {
    FILE* fd = fopen(path.data(), "rb");
    if (fd == nullptr) {
        logger::error("dump_index_chunk: fopen %s failed (errno=%d: %s)",
                      path.data(), errno, strerror(errno));
        return;
    }

    // ---- URL section --------------------------------------------------
    //
    // Layout:
    //   <8B urls_bytes>\n
    //   <4B id> <1B space> <URL> \n       (repeated; 6 + url_len bytes each)
    //   \n                                (separator)
    uint64_t urls_bytes = 0;
    fread(&urls_bytes, sizeof(uint64_t), 1, fd);
    fgetc(fd); // '\n'

    logger::instr("=== URL section (urls_bytes=%llu) ===", (unsigned long long)urls_bytes);
    uint64_t consumed = 0;
    size_t   num_urls = 0;
    while (consumed < urls_bytes) {
        // Cannot use fgets here: an ID byte may be 0x0A which fgets would
        // (incorrectly) treat as end-of-line. Read the fixed-size fields
        // explicitly and then scan byte-by-byte until the terminating \n.
        uint32_t id;
        if (fread(&id, sizeof(uint32_t), 1, fd) != 1) break;
        char sp;
        if (fread(&sp, 1, 1, fd) != 1) break; // ' '

        size_t url_len = 0;
        char   url_buf[2048];
        while (url_len < sizeof(url_buf)) {
            int c = fgetc(fd);
            if (c == EOF || c == '\n') break;
            url_buf[url_len++] = static_cast<char>(c);
        }
        logger::instr("  id=%u -> %.*s", id, (int)url_len, url_buf);
        consumed += 4 + 1 + url_len + 1;
        num_urls++;
    }
    fgetc(fd); // separator '\n'
    logger::instr("  (%zu URLs)", num_urls);

    // ---- Dictionary lookup table --------------------------------------
    //
    // Layout (36 fixed entries: 26 letters + 10 digits):
    //   <1B char> <8B offset>\n
    //   \n                       (separator)
    logger::instr("=== Dictionary ToC (char -> byte offset into dict) ===");
    constexpr int TOC_ENTRIES = 36;
    for (int i = 0; i < TOC_ENTRIES; ++i) {
        char     letter, sp;
        uint64_t off;
        fread(&letter, 1, 1, fd);
        fread(&sp,     1, 1, fd); // ' '
        fread(&off,    sizeof(uint64_t), 1, fd);
        fread(&sp,     1, 1, fd); // '\n'
        logger::instr("  %c -> %llu", letter, (unsigned long long)off);
    }
    fgetc(fd); // separator '\n'

    // ---- Dictionary ---------------------------------------------------
    //
    // Layout:
    //   <varlen word> <8B posting_offset>\n   (repeated, sorted)
    //   \n                                    (separator)
    //
    // The 8-byte offset can legitimately contain the byte 0x0A, so we
    // can't use fgets here — parse byte-by-byte using the space/newline
    // structure.
    vector<DictEntry> dict;
    while (true) {
        char c;
        if (fread(&c, 1, 1, fd) != 1) break;
        if (c == '\n') break; // separator reached

        char   word_buf[256];
        size_t wlen = 0;
        word_buf[wlen++] = c;
        while (fread(&c, 1, 1, fd) == 1 && c != ' ') {
            if (wlen < sizeof(word_buf)) word_buf[wlen++] = c;
        }
        uint64_t off;
        fread(&off, sizeof(uint64_t), 1, fd);
        fgetc(fd); // '\n'

        DictEntry e { string(word_buf, wlen), off };
        dict.push_back(static_cast<DictEntry&&>(e));
    }

    logger::instr("=== Dictionary (%zu words, showing first %zu) ===",
                  dict.size(), bench::DUMP_DICT_WORDS);
    for (size_t i = 0; i < dict.size() && i < bench::DUMP_DICT_WORDS; ++i) {
        logger::instr("  %.*s -> %llu",
                      (int)dict[i].word.size(), dict[i].word.data(),
                      (unsigned long long)dict[i].posting_offset);
    }
    if (dict.size() > bench::DUMP_DICT_WORDS) {
        logger::instr("  ... (%zu more)", dict.size() - bench::DUMP_DICT_WORDS);
    }

    // ---- Posting lists ------------------------------------------------
    //
    // Layout for each word (in dict order):
    //   <8B num_posts> <4B n_docs>\n
    //   <skip list, padded to SKIP_LIST_SIZE bytes>
    //   <posts>
    //   \n
    //
    // Skip list: each entry is <4B doc_id> <1B space> <8B offset> \n
    // (SKIP_LIST_ENTRY_SIZE = 14 bytes). persist() writes one entry per
    // new doc PLUS one per checkpoint crossed (multiples of
    // INDEX_SKIP_SIZE), then zero-pads the region up to SKIP_LIST_SIZE so
    // the dict offsets computed in the first pass stay consistent with
    // the on-disk layout. Valid entries are parsed until we hit a zero
    // doc_id (doc IDs are 1-indexed, so 0 unambiguously marks padding).
    //
    // Posts: each post is either
    //   <0x00 flag><utf8 doc_delta><utf8 loc_delta>   (new doc)
    //   <utf8 loc_delta>                              (same doc)
    // A UTF-8 encoding of a positive loc_delta never begins with 0x00,
    // so the flag byte is unambiguous.
    const size_t SKIP_LIST_ENTRY_SIZE = 4 + 1 + 8 + 1;
    const size_t SKIP_LIST_SIZE =
        (DOCS_PER_INDEX_CHUNK / INDEX_SKIP_SIZE) * SKIP_LIST_ENTRY_SIZE;

    logger::instr("=== Posting lists (showing first %zu) ===", bench::DUMP_POSTING_LISTS);
    for (size_t i = 0; i < dict.size(); ++i) {
        uint64_t num_posts = 0;
        uint32_t n_docs    = 0;
        char     sp;
        fread(&num_posts, sizeof(uint64_t), 1, fd);
        fread(&sp,        1, 1, fd); // ' '
        fread(&n_docs,    sizeof(uint32_t), 1, fd);
        fread(&sp,        1, 1, fd); // '\n'

        bool verbose = (i < bench::DUMP_POSTING_LISTS);
        if (verbose) {
            logger::instr("  [%.*s] num_posts=%llu n_docs=%u",
                          (int)dict[i].word.size(), dict[i].word.data(),
                          (unsigned long long)num_posts, n_docs);
            logger::instr("    skip list (SKIP_LIST_SIZE=%zu bytes, showing first %zu valid entries):",
                          SKIP_LIST_SIZE, bench::DUMP_SKIPS_PER_LIST);
        }

        // Pull the entire skip list region into memory so we can parse
        // valid entries and then discard the zero padding in one pass.
        static Utf8 skip_buf[(DOCS_PER_INDEX_CHUNK / INDEX_SKIP_SIZE) * 14];
        if (fread(skip_buf, 1, SKIP_LIST_SIZE, fd) != SKIP_LIST_SIZE) {
            logger::error("skip list EOF for word %.*s",
                          (int)dict[i].word.size(), dict[i].word.data());
            break;
        }
        size_t cursor = 0;
        size_t valid_entries = 0;
        while (cursor + SKIP_LIST_ENTRY_SIZE <= SKIP_LIST_SIZE) {
            uint32_t doc_id;
            memcpy(&doc_id, skip_buf + cursor, sizeof(uint32_t));
            if (doc_id == 0) break; // padding
            uint64_t off;
            memcpy(&off, skip_buf + cursor + 5, sizeof(uint64_t));
            if (verbose && valid_entries < bench::DUMP_SKIPS_PER_LIST) {
                logger::instr("      doc=%u -> offset=%llu",
                              doc_id, (unsigned long long)off);
            }
            cursor += SKIP_LIST_ENTRY_SIZE;
            valid_entries++;
        }
        if (verbose) {
            logger::instr("    (%zu valid skip entries, rest is zero padding)",
                          valid_entries);
        }

        if (verbose) {
            logger::instr("    posts (showing first %zu):", bench::DUMP_POSTS_PER_LIST);
        }
        uint32_t last_doc = 0;
        uint32_t last_loc = 0;
        for (uint64_t p = 0; p < num_posts; ++p) {
            Utf8 first;
            if (fread(&first, 1, 1, fd) != 1) { logger::error("posts EOF"); break; }

            uint32_t doc;
            if (first == 0x00) {
                Unicode doc_delta = read_utf8_from_file(fd);
                doc = last_doc + static_cast<uint32_t>(doc_delta);
                last_doc = doc;
                last_loc = 0;
            } else {
                ungetc(first, fd);
                doc = last_doc;
            }

            Unicode loc_delta = read_utf8_from_file(fd);
            uint32_t loc = last_loc + static_cast<uint32_t>(loc_delta);
            last_loc = loc;

            if (verbose && p < bench::DUMP_POSTS_PER_LIST) {
                logger::instr("      doc=%u loc=%u", doc, loc);
            }
        }

        fgetc(fd); // trailing '\n' for this posting list
    }

    fclose(fd);
}


// ---- Rebuild + verify -----------------------------------------------------
//
// Walk the on-disk index and reconstruct, for every (doc, loc), the word that
// was written at that position. Then diff the reconstruction against the
// in-memory Corpus that generated the parser files.

static uint32_t find_pool_index(const WordPool& pool, const char* word, size_t len) {
    for (size_t i = 0; i < pool.words.size(); ++i) {
        if (pool.words[i].size() == len &&
            memcmp(pool.words[i].data(), word, len) == 0) {
            return static_cast<uint32_t>(i);
        }
    }
    return UINT32_MAX;
}


static vector<vector<uint32_t>> rebuild_docs_from_index(
        const string& path, const WordPool& pool,
        size_t total_docs, size_t words_per_doc) {

    vector<vector<uint32_t>> rebuilt;
    rebuilt.reserve(total_docs);
    for (size_t d = 0; d < total_docs; ++d) {
        vector<uint32_t> row;
        row.reserve(words_per_doc);
        for (size_t l = 0; l < words_per_doc; ++l) row.push_back(UINT32_MAX);
        rebuilt.push_back(static_cast<vector<uint32_t>&&>(row));
    }

    FILE* fd = fopen(path.data(), "rb");
    if (fd == nullptr) {
        logger::error("rebuild: fopen %s failed (errno=%d: %s)",
                      path.data(), errno, strerror(errno));
        return rebuilt;
    }

    // Skip the URL section: <8B urls_bytes>\n then urls_bytes of data then \n
    uint64_t urls_bytes = 0;
    fread(&urls_bytes, sizeof(uint64_t), 1, fd);
    fgetc(fd); // '\n'
    fseek(fd, static_cast<long>(urls_bytes), SEEK_CUR);
    fgetc(fd); // separator '\n'

    // Skip the dict ToC: INDEX_DICTIONARY_TOC_SIZE bytes, then \n separator.
    fseek(fd, static_cast<long>(INDEX_DICTIONARY_TOC_SIZE), SEEK_CUR);
    fgetc(fd); // separator '\n'

    // Parse the dictionary to get the word list in posting-list order.
    vector<DictEntry> dict;
    while (true) {
        char c;
        if (fread(&c, 1, 1, fd) != 1) break;
        if (c == '\n') break;

        char   word_buf[256];
        size_t wlen = 0;
        word_buf[wlen++] = c;
        while (fread(&c, 1, 1, fd) == 1 && c != ' ') {
            if (wlen < sizeof(word_buf)) word_buf[wlen++] = c;
        }
        uint64_t off;
        fread(&off, sizeof(uint64_t), 1, fd);
        fgetc(fd); // '\n'

        DictEntry e { string(word_buf, wlen), off };
        dict.push_back(static_cast<DictEntry&&>(e));
    }

    const size_t SKIP_LIST_ENTRY_SIZE = 4 + 1 + 8 + 1;
    // const size_t SKIP_LIST_SIZE =
    //     (DOCS_PER_INDEX_CHUNK / INDEX_SKIP_SIZE) * SKIP_LIST_ENTRY_SIZE;
    const size_t SKIP_LIST_SIZE = 0;

    // Walk every posting list and populate rebuilt[doc-1][loc-1].
    for (size_t i = 0; i < dict.size(); ++i) {
        uint64_t num_posts = 0;
        uint32_t n_docs    = 0;
        char     sp;
        fread(&num_posts, sizeof(uint64_t), 1, fd);
        fread(&sp,        1, 1, fd);
        fread(&n_docs,    sizeof(uint32_t), 1, fd);
        fread(&sp,        1, 1, fd);

        fseek(fd, static_cast<long>(SKIP_LIST_SIZE), SEEK_CUR);

        uint32_t pool_idx = find_pool_index(pool,
                                            dict[i].word.data(),
                                            dict[i].word.size());
        if (pool_idx == UINT32_MAX) {
            logger::error("rebuild: dict word '%.*s' not found in pool",
                          (int)dict[i].word.size(), dict[i].word.data());
        }

        uint32_t last_doc = 0;
        uint32_t last_loc = 0;
        for (uint64_t p = 0; p < num_posts; ++p) {
            Utf8 first;
            if (fread(&first, 1, 1, fd) != 1) { logger::error("rebuild: posts EOF"); break; }

            uint32_t doc;
            if (first == 0x00) {
                Unicode doc_delta = read_utf8_from_file(fd);
                doc = last_doc + static_cast<uint32_t>(doc_delta);
                last_doc = doc;
                last_loc = 0;
            } else {
                ungetc(first, fd);
                doc = last_doc;
            }

            Unicode loc_delta = read_utf8_from_file(fd);
            uint32_t loc = last_loc + static_cast<uint32_t>(loc_delta);
            last_loc = loc;

            size_t d_idx = static_cast<size_t>(doc) - 1;
            size_t l_idx = static_cast<size_t>(loc) - 1;
            if (d_idx < rebuilt.size() && l_idx < rebuilt[d_idx].size()) {
                rebuilt[d_idx][l_idx] = pool_idx;
            }
        }

        fgetc(fd); // trailing '\n'
    }

    fclose(fd);
    return rebuilt;
}


// Same reconstruction as rebuild_docs_from_index, but goes through the public
// LoadedIndex/IndexStreamReader interface rather than parsing the chunk file
// directly. This exercises the ISR end-to-end: it must walk every post for
// every word in the dictionary and place it at the exact (doc, loc) that the
// corpus wrote.
static vector<vector<uint32_t>> rebuild_docs_from_isr(
        const string& path, const WordPool& pool,
        size_t total_docs, size_t words_per_doc) {

    vector<vector<uint32_t>> rebuilt;
    rebuilt.reserve(total_docs);
    for (size_t d = 0; d < total_docs; ++d) {
        vector<uint32_t> row;
        row.reserve(words_per_doc);
        for (size_t l = 0; l < words_per_doc; ++l) row.push_back(UINT32_MAX);
        rebuilt.push_back(static_cast<vector<uint32_t>&&>(row));
    }

    LoadedIndex loaded(path);
    const size_t n_words = loaded.num_words();
    logger::instr("ISR rebuild: %zu dictionary words loaded", n_words);

    size_t total_posts = 0;
    for (size_t i = 0; i < n_words; ++i) {
        string word = loaded.word_at(i);

        uint32_t pool_idx = find_pool_index(pool, word.data(), word.size());
        if (pool_idx == UINT32_MAX) {
            logger::error("ISR rebuild: dict word '%.*s' not found in pool",
                          (int)word.size(), word.data());
            continue;
        }

        // IndexStreamReader takes `string word` by value (move-only type), so
        // construct a fresh copy from the dict word for the ISR to own.
        IndexStreamReader isr(string(word.data(), word.size()), &loaded);

        post p = isr.advance();
        while (p.doc != 0) {
            total_posts++;
            size_t d_idx = static_cast<size_t>(p.doc) - 1;
            size_t l_idx = static_cast<size_t>(p.loc) - 1;
            if (d_idx < rebuilt.size() && l_idx < rebuilt[d_idx].size()) {
                rebuilt[d_idx][l_idx] = pool_idx;
            } else {
                logger::error("ISR rebuild: out-of-range post doc=%u loc=%u for '%.*s'",
                              p.doc, p.loc,
                              (int)word.size(), word.data());
            }
            p = isr.advance();
        }
    }

    logger::instr("ISR rebuild: walked %zu posts across %zu words",
                  total_posts, n_words);
    return rebuilt;
}


static void verify_rebuild(const Corpus& corpus, const WordPool& pool,
                           const vector<vector<uint32_t>>& rebuilt) {
    const size_t MAX_REPORT = 10;
    size_t mismatches = 0;
    size_t unset      = 0;
    size_t total      = 0;

    for (size_t d = 0; d < corpus.doc_words.size(); ++d) {
        const vector<uint32_t>& expected = corpus.doc_words[d];
        const vector<uint32_t>& actual   = rebuilt[d];
        for (size_t l = 0; l < expected.size(); ++l) {
            total++;
            uint32_t got = actual[l];
            uint32_t want = expected[l];
            if (got == UINT32_MAX) {
                if (unset < MAX_REPORT) {
                    const string& w = pool.words[want];
                    logger::error("  doc=%zu loc=%zu missing (expected '%.*s')",
                                  d, l, (int)w.size(), w.data());
                }
                unset++;
            } else if (got != want) {
                // Pool indices may differ while the underlying word string
                // is identical (the random word pool isn't de-duplicated and
                // the rebuild path resolves each word to the first matching
                // pool entry). Only count true string-level mismatches.
                const string& gw = pool.words[got];
                const string& ww = pool.words[want];
                if (gw.size() != ww.size() ||
                    memcmp(gw.data(), ww.data(), gw.size()) != 0) {
                    if (mismatches < MAX_REPORT) {
                        logger::error("  doc=%zu loc=%zu: got '%.*s', expected '%.*s'",
                                      d, l,
                                      (int)gw.size(), gw.data(),
                                      (int)ww.size(), ww.data());
                    }
                    mismatches++;
                }
            }
        }
    }

    if (mismatches == 0 && unset == 0) {
        logger::instr("Rebuild verification PASSED (%zu positions checked)", total);
    } else {
        logger::error("Rebuild verification FAILED: %zu mismatches, %zu unset (of %zu)",
                      mismatches, unset, total);
    }
}


int main(int /*argc*/, char* /*argv*/[]) {
    logger::instr("index-isr-validation: starting");

    cleanup_dir(bench::BENCH_DOC_DIR);
    mkdir_p(INDEX_OUTPUT_DIR);
    cleanup_worker0_chunks();

    // Word pool: fixed seed. Same 36 buckets every run so the dictionary
    // under test is stable.
    std::mt19937_64 pool_rng(bench::POOL_RNG_SEED);
    WordPool pool = build_word_pool(pool_rng);
    logger::instr("Built word pool: 36 buckets (26 letters + 10 digits) x %zu words", bench::WORDS_PER_LETTER);
    log_word_pool(pool);

    // Corpus: non-deterministic seed drawn from random_device so each run
    // exercises the dictionary against a different set of documents. The
    // seed is logged so a specific run can be reproduced on failure.
    std::random_device rd;
    uint64_t corpus_seed = (static_cast<uint64_t>(rd()) << 32) | rd();
    logger::instr("Corpus RNG seed: 0x%016llx", (unsigned long long)corpus_seed);
    std::mt19937_64 corpus_rng(corpus_seed);

    Corpus corpus = generate_corpus(pool, corpus_rng);
    logger::instr("Generated corpus: %zu files x %zu docs x %zu words",
                  bench::NUM_FILES, bench::DOCS_PER_FILE, bench::WORDS_PER_DOC);

    write_corpus_files(pool, corpus);

    BenchTimings t = run_index_worker();
    const size_t total_docs  = bench::NUM_FILES * bench::DOCS_PER_FILE;
    const size_t total_words = total_docs * bench::WORDS_PER_DOC;
    logger::instr("Index (in-mem):  %.3f ms", t.index_ms);
    logger::instr("Flush (persist): %.3f ms", t.flush_ms);
    logger::instr("Total:           %.3f ms  (%zu docs, %zu words)",
                  t.index_ms + t.flush_ms, total_docs, total_words);

    string chunk_path = index_chunk_path(0, 0);
    dump_index_chunk(chunk_path);

    logger::instr("=== Rebuilding documents from index (file-walk) ===");
    vector<vector<uint32_t>> rebuilt = rebuild_docs_from_index(
        chunk_path, pool, total_docs, bench::WORDS_PER_DOC);
    verify_rebuild(corpus, pool, rebuilt);

    logger::instr("=== Rebuilding documents via ISR ===");
    vector<vector<uint32_t>> rebuilt_isr = rebuild_docs_from_isr(
        chunk_path, pool, total_docs, bench::WORDS_PER_DOC);
    verify_rebuild(corpus, pool, rebuilt_isr);

    cleanup_dir(bench::BENCH_DOC_DIR);
    cleanup_worker0_chunks();

    return 0;
}
