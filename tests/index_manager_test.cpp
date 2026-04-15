#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <random>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

#include "index/Index.h"
#include "index_chunk/chunk_manager.h"
#include "index_manager/index_manager.h"
#include "lib/consts.h"
#include "lib/query_response.h"
#include "lib/string.h"
#include "lib/vector.h"


namespace {

constexpr const char* PARSER_DIR            = "/tmp/index_manager_test";
constexpr size_t      NUM_WORKERS           = 3;
constexpr size_t      FILES_PER_WORKER      = 2;
constexpr size_t      NUM_FILES             = NUM_WORKERS * FILES_PER_WORKER;
constexpr size_t      DOCS_PER_FILE         = 15;
constexpr size_t      WORDS_PER_DOC         = 120;
// Keep the per-doc distinct vocabulary small so multi-word AND queries aren't
// trivially universal.
constexpr size_t      DISTINCT_WORDS_PER_DOC = 6;
constexpr uint64_t    RNG_SEED              = 0xF1A5C0DE4BULL;


// Same pool as index_util_test: pure [a-z0-9] so every word survives
// IndexChunk::sort_entries' filter.
static vector<string> make_word_pool() {

    // Doing tricks on it fr
    const char* words[] = {
        "apple", "banana", "cherry", "date", "elder", "fig", "grape", "honey",
        "iris", "jade", "kiwi", "lemon", "mango", "nectar", "olive", "peach",
        "quince", "raisin", "sage", "thyme", "umber", "violet", "walnut",
        "xenon", "yam", "zebra", "0cool", "1up", "2fast", "3lite",
    };
    vector<string> pool;
    const size_t n = sizeof(words) / sizeof(words[0]);
    pool.reserve(n);
    for (size_t i = 0; i < n; ++i) pool.push_back(string(words[i]));
    return pool;
}


struct Corpus {
    vector<string>             urls;
    vector<vector<uint32_t>>   doc_words;
    // word_to_docs[pool_idx] = set of GLOBAL 1-indexed doc ids containing the
    // word. (global across all workers)
    vector<std::set<uint32_t>> word_to_docs;
};


static Corpus generate_corpus(const vector<string>& pool, std::mt19937_64& rng) {
    Corpus c;
    const size_t total_docs = NUM_FILES * DOCS_PER_FILE;
    c.urls.reserve(total_docs);
    c.doc_words.reserve(total_docs);
    assert(DISTINCT_WORDS_PER_DOC <= pool.size());

    vector<uint32_t> idx_pool;
    idx_pool.reserve(pool.size());
    for (size_t i = 0; i < pool.size(); ++i) {
        idx_pool.push_back(static_cast<uint32_t>(i));
    }

    std::uniform_int_distribution<uint32_t> subset_pick(0, DISTINCT_WORDS_PER_DOC - 1);

    char url_buf[64];
    for (size_t f = 0; f < NUM_FILES; ++f) {
        for (size_t d = 0; d < DOCS_PER_FILE; ++d) {
            int n = snprintf(url_buf, sizeof(url_buf), "http://test.local/f%zu/d%zu", f, d);
            c.urls.push_back(string(url_buf, static_cast<size_t>(n)));

            // Partial Fisher-Yates to pick a distinct subset per doc.
            for (size_t i = 0; i < DISTINCT_WORDS_PER_DOC; ++i) {
                std::uniform_int_distribution<size_t> pick(i, pool.size() - 1);
                size_t j = pick(rng);
                uint32_t tmp = idx_pool[i];
                idx_pool[i]  = idx_pool[j];
                idx_pool[j]  = tmp;
            }

            vector<uint32_t> row;
            row.reserve(WORDS_PER_DOC);
            for (size_t w = 0; w < WORDS_PER_DOC; ++w) {
                uint32_t pidx = idx_pool[subset_pick(rng)];
                row.push_back(pidx);
            }
            c.doc_words.push_back(static_cast<vector<uint32_t>&&>(row));
        }
    }
    return c;
}


// Each worker restarts its local doc id from 1, so the corpus' global doc ids
// don't match the per-chunk ids. Build a url-keyed ground truth instead: a
// word maps to the set of URLs that contain it anywhere in the corpus.
static vector<std::set<std::string>> build_url_ground_truth(const vector<string>& pool,
                                                            const Corpus& corpus) {
    vector<std::set<std::string>> word_to_urls;
    word_to_urls.resize(pool.size());
    const size_t total_docs = corpus.doc_words.size();
    for (size_t d = 0; d < total_docs; ++d) {
        const vector<uint32_t>& row = corpus.doc_words[d];
        std::set<uint32_t> in_doc;
        for (size_t l = 0; l < row.size(); ++l) in_doc.insert(row[l]);
        for (uint32_t pidx : in_doc) {
            const string& url = corpus.urls[d];
            word_to_urls[pidx].insert(std::string(url.data(), url.size()));
        }
    }
    return word_to_urls;
}


// mkdir -p: create each component, ignoring EEXIST.
static void mkdir_p(const char* path) {
    char buf[512];
    size_t n = strlen(path);
    assert(n < sizeof(buf));
    memcpy(buf, path, n + 1);
    for (size_t i = 1; i < n; ++i) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            mkdir(buf, 0755);
            buf[i] = '/';
        }
    }
    mkdir(buf, 0755);
}


// Delete every regular file in `dir` whose name starts with `prefix`.
// null prefix -> clear dir
static void clean_dir(const char* dir, const char* prefix) {
    DIR* d = opendir(dir);
    if (!d) return;
    char path[1024];
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        if (prefix && strncmp(e->d_name, prefix, strlen(prefix)) != 0) continue;
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        unlink(path);
    }
    closedir(d);
}


static string parser_file_path(size_t file_idx) {
    return string::join("", string(PARSER_DIR), "/docs_", string(static_cast<uint32_t>(file_idx)), ".txt");
}


// Parser output format consumed by IndexChunk::index_file:
//   <doc>\n<url>\n word\n word\n ... </doc>\n
static void write_parser_files(const vector<string>& pool, const Corpus& c) {
    mkdir_p(PARSER_DIR);
    for (size_t f = 0; f < NUM_FILES; ++f) {
        string path = parser_file_path(f);
        FILE* fd = fopen(path.data(), "w");
        assert(fd != nullptr);

        for (size_t d = 0; d < DOCS_PER_FILE; ++d) {
            size_t g = f * DOCS_PER_FILE + d;
            fputs("<doc>\n", fd);
            fwrite(c.urls[g].data(), 1, c.urls[g].size(), fd);
            fputc('\n', fd);
            const vector<uint32_t>& row = c.doc_words[g];
            for (size_t w = 0; w < row.size(); ++w) {
                const string& word = pool[row[w]];
                fwrite(word.data(), 1, word.size(), fd);
                fputc('\n', fd);
            }
            fputs("</doc>\n", fd);
        }
        fclose(fd);
    }
}


// Split files across NUM_WORKERS IndexChunks so the on-disk layout looks like
// what index_manager expects to discover: multiple workers, each with one
// chunk file.
static void build_multi_worker_index() {
    for (uint32_t w = 0; w < NUM_WORKERS; ++w) {
        IndexChunk idx(w);
        for (size_t i = 0; i < FILES_PER_WORKER; ++i) {
            size_t f = w * FILES_PER_WORKER + i;
            assert(idx.index_file(parser_file_path(f)));
        }
        idx.flush();
    }
}


// Linear scan: find the pool index whose string matches `word`. Returns
// UINT32_MAX if not in the pool.
static uint32_t pool_idx_for_word(const vector<string>& pool, const string& w) {
    for (size_t k = 0; k < pool.size(); ++k) {
        if (pool[k].size() == w.size() && memcmp(pool[k].data(), w.data(), w.size()) == 0) {
            return static_cast<uint32_t>(k);
        }
    }
    return UINT32_MAX;
}


// Intersect URL sets across every query word.
static std::set<std::string> expected_urls(const vector<std::set<std::string>>& word_to_urls,
                                           const vector<uint32_t>& pool_idxs) {
    std::set<std::string> out;
    if (pool_idxs.size() == 0) return out;
    out = word_to_urls[pool_idxs[0]];
    for (size_t i = 1; i < pool_idxs.size(); ++i) {
        const std::set<std::string>& other = word_to_urls[pool_idxs[i]];
        std::set<std::string> next;
        for (const std::string& u : out) {
            if (other.count(u)) next.insert(u);
        }
        out = next;
    }
    return out;
}


void test_index_manager_discovery_and_query() {
    constexpr size_t NUM_QUERIES = 150;

    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    // Index manager walks EVERY worker slot, so wipe every index_chunk_ file
    // (not just worker 0) to guarantee it only sees what this test wrote.
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_");

    std::mt19937_64 rng(RNG_SEED);
    vector<string> pool = make_word_pool();
    Corpus corpus = generate_corpus(pool, rng);

    write_parser_files(pool, corpus);
    build_multi_worker_index();

    // Sanity-check that the on-disk files are where we expect them, so a
    // regression in path-building shows up here rather than as an empty
    // index_manager.
    for (uint32_t w = 0; w < NUM_WORKERS; ++w) {
        string p = string::join("", string(INDEX_OUTPUT_DIR), "/index_chunk_", string(w), "_0.txt");
        struct stat st;
        assert(stat(p.data(), &st) == 0);
    }

    vector<std::set<std::string>> word_to_urls = build_url_ground_truth(pool, corpus);

    index_manager im;

    // Relax the per-chunk deadline so leapfrog never returns partial results
    // on a slow CI runner — matches what index_util_test does.
    chunk_manager::deadline_ms = 1000;

    std::uniform_int_distribution<uint32_t> pdist(0, pool.size() - 1);
    std::uniform_int_distribution<size_t>   qlen(1, 4);

    size_t nonempty = 0;
    size_t empty    = 0;

    for (size_t q = 0; q < NUM_QUERIES; ++q) {
        size_t k = qlen(rng);
        vector<uint32_t> pool_idxs;
        vector<string>   words;
        pool_idxs.reserve(k);
        words.reserve(k);

        // Sample without replacement — default_query requires unique words.
        std::set<uint32_t> seen;
        while (pool_idxs.size() < k) {
            uint32_t p = pdist(rng);
            if (seen.insert(p).second) {
                pool_idxs.push_back(p);
                words.push_back(string(pool[p].data(), pool[p].size()));
            }
        }

        std::set<std::string> want = expected_urls(word_to_urls, pool_idxs);
        QueryResponse resp = im.handle_query(words);

        assert(resp.pages.size() == want.size());

        std::set<std::string> got;
        for (const DocInfo& di : resp.pages) {
            got.insert(std::string(di.url.data(), di.url.size()));
        }
        assert(got == want);

        // Each DocInfo must carry positions for every queried word.
        for (const DocInfo& di : resp.pages) {
            assert(di.wordInfo.size() == words.size());
            // Every word's pos list must be non-empty (the doc matched the
            // AND query, so every word has at least one hit).
            for (const WordInfo& wi : di.wordInfo) {
                assert(wi.pos.size() > 0);
                uint32_t pidx = pool_idx_for_word(pool, wi.word);
                assert(pidx != UINT32_MAX);
            }
        }

        if (want.empty()) empty++;
        else              nonempty++;
    }

    assert(nonempty > 0);
    assert(empty    > 0);

    // A missing word must fan out, hit every chunk's short-circuit, and
    // return an empty QueryResponse.
    {
        vector<string> only_missing;
        only_missing.push_back(string("NONEXISTENTWORD"));
        QueryResponse resp = im.handle_query(only_missing);
        assert(resp.pages.size() == 0);
    }

    // Try AND query with a nonexistenbt and a real, must return empty query.
    {
        vector<string> mixed;
        mixed.push_back(string(pool[0].data(), pool[0].size()));
        mixed.push_back(string("NONEXISTENTWORD"));
        QueryResponse resp = im.handle_query(mixed);
        assert(resp.pages.size() == 0);
    }

    printf("  queries: %zu non-empty, %zu empty (across %zu chunks)\n",
           nonempty, empty, NUM_WORKERS);

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_");
}


// An index_manager constructed against an empty INDEX_OUTPUT_DIR shouldn't crash.
void test_index_manager_empty_dir() {
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_");

    index_manager im;

    vector<string> words;
    words.push_back(string("apple"));
    QueryResponse resp = im.handle_query(words);
    assert(resp.pages.size() == 0);
}

}


int main() {
    printf("\n=== RUNNING INDEX MANAGER TESTS ===\n\n");

    test_index_manager_empty_dir();
    test_index_manager_discovery_and_query();

    printf("\n=== ALL INDEX MANAGER TESTS FINISHED ===\n\n");
    return 0;
}
