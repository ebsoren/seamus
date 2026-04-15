#include <cassert>
#include <cerrno>
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
#include "lib/consts.h"
#include "lib/string.h"
#include "lib/vector.h"


namespace {

constexpr const char* PARSER_DIR             = "/tmp/index_util_test";
constexpr size_t      NUM_FILES               = 5;
constexpr size_t      DOCS_PER_FILE           = 20;
constexpr size_t      WORDS_PER_DOC           = 200;
// Each doc draws only DISTINCT_WORDS_PER_DOC unique words from the pool and
// fills its WORDS_PER_DOC positions from that subset. Without this, uniform
// sampling across a small pool makes every word land in every doc, so every
// multi-word AND query trivially returns the full corpus.
constexpr size_t      DISTINCT_WORDS_PER_DOC  = 6;
constexpr uint64_t    RNG_SEED                = 0xC0FFEEULL;


// Fixed word pool: each word is pure [a-z0-9] so it survives IndexChunk's
// sort_entries filter. Small enough that most words get dozens of posts.
static vector<string> make_word_pool() {
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
    vector<string>             urls;         // urls[g] for global doc g
    vector<vector<uint32_t>>   doc_words;    // doc_words[g][l] = pool index
    // word_to_docs[pool_idx] = set of 1-indexed doc ids that contain the word.
    // Ground truth for AND-query intersection checks.
    vector<std::set<uint32_t>> word_to_docs;
};


static Corpus generate_corpus(const vector<string>& pool, std::mt19937_64& rng) {
    Corpus c;
    const size_t total_docs = NUM_FILES * DOCS_PER_FILE;
    c.urls.reserve(total_docs);
    c.doc_words.reserve(total_docs);
    c.word_to_docs.resize(pool.size());
    assert(DISTINCT_WORDS_PER_DOC <= pool.size());

    // For per-doc subset sampling: shuffle a working copy of pool indices and
    // take the first DISTINCT_WORDS_PER_DOC. Fisher-Yates keeps this O(k) per
    // doc since we only need the prefix.
    vector<uint32_t> idx_pool;
    idx_pool.reserve(pool.size());
    for (size_t i = 0; i < pool.size(); ++i) {
        idx_pool.push_back(static_cast<uint32_t>(i));
    }

    std::uniform_int_distribution<uint32_t> subset_pick(
        0, DISTINCT_WORDS_PER_DOC - 1);

    char url_buf[64];
    for (size_t f = 0; f < NUM_FILES; ++f) {
        for (size_t d = 0; d < DOCS_PER_FILE; ++d) {
            int n = snprintf(url_buf, sizeof(url_buf),
                             "http://test.local/f%zu/d%zu", f, d);
            c.urls.push_back(string(url_buf, static_cast<size_t>(n)));

            const uint32_t doc_id = static_cast<uint32_t>(
                f * DOCS_PER_FILE + d + 1); // 1-indexed, matches IndexChunk

            // Partial Fisher-Yates: pick DISTINCT_WORDS_PER_DOC indices from
            // idx_pool by swapping into the prefix. Subsequent docs see a
            // permuted pool but that's fine — we're sampling uniformly.
            for (size_t i = 0; i < DISTINCT_WORDS_PER_DOC; ++i) {
                std::uniform_int_distribution<size_t> pick(i, pool.size() - 1);
                size_t j = pick(rng);
                uint32_t tmp = idx_pool[i];
                idx_pool[i] = idx_pool[j];
                idx_pool[j] = tmp;
            }

            vector<uint32_t> row;
            row.reserve(WORDS_PER_DOC);
            for (size_t w = 0; w < WORDS_PER_DOC; ++w) {
                uint32_t pidx = idx_pool[subset_pick(rng)];
                row.push_back(pidx);
                c.word_to_docs[pidx].insert(doc_id);
            }
            c.doc_words.push_back(static_cast<vector<uint32_t>&&>(row));
        }
    }
    return c;
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
            mkdir(buf, 0755);  // ignore EEXIST
            buf[i] = '/';
        }
    }
    mkdir(buf, 0755);
}


// Delete every regular file in `dir` whose name starts with `prefix`.
// If prefix is nullptr, delete everything.
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
    return string::join("",
                        string(PARSER_DIR),
                        "/docs_",
                        string(static_cast<uint32_t>(file_idx)),
                        ".txt");
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


// Single-threaded indexer: worker 0 sweeps every parser file and flushes.
static string build_index_chunk() {
    IndexChunk idx(0);
    for (size_t f = 0; f < NUM_FILES; ++f) {
        assert(idx.index_file(parser_file_path(f)));
    }
    idx.flush();

    return string::join("",
                        string(INDEX_OUTPUT_DIR),
                        "/index_chunk_0_0.txt");
}


// Rebuild doc_words[g][l] from the loaded index by walking each dict word's
// ISR. Every post must land at a position the corpus wrote; a UINT32_MAX slot
// after the walk means the indexer lost that position.
static void verify_loaded_index(const string& chunk_path,
                                const vector<string>& pool,
                                const Corpus& corpus) {
    LoadedIndex loaded(chunk_path);
    const size_t n_words    = loaded.num_words();
    const size_t total_docs = corpus.doc_words.size();

    assert(n_words > 0);

    vector<vector<uint32_t>> rebuilt;
    rebuilt.reserve(total_docs);
    for (size_t d = 0; d < total_docs; ++d) {
        vector<uint32_t> row;
        row.reserve(WORDS_PER_DOC);
        for (size_t l = 0; l < WORDS_PER_DOC; ++l) row.push_back(UINT32_MAX);
        rebuilt.push_back(static_cast<vector<uint32_t>&&>(row));
    }

    size_t total_posts = 0;
    for (size_t i = 0; i < n_words; ++i) {
        string word = loaded.word_at(i);

        // Map the dict word back to a pool index so we can compare by value.
        uint32_t pool_idx = UINT32_MAX;
        for (size_t k = 0; k < pool.size(); ++k) {
            if (pool[k].size() == word.size() &&
                memcmp(pool[k].data(), word.data(), word.size()) == 0) {
                pool_idx = static_cast<uint32_t>(k);
                break;
            }
        }
        assert(pool_idx != UINT32_MAX);

        IndexStreamReader isr(string(word.data(), word.size()), &loaded);
        post p = isr.advance();
        while (p.doc != 0) {
            size_t d_idx = static_cast<size_t>(p.doc) - 1;
            size_t l_idx = static_cast<size_t>(p.loc) - 1;
            assert(d_idx < total_docs);
            assert(l_idx < WORDS_PER_DOC);
            rebuilt[d_idx][l_idx] = pool_idx;
            total_posts++;
            p = isr.advance();
        }
    }

    // Every (doc, loc) in the corpus must be reconstructed exactly.
    for (size_t d = 0; d < total_docs; ++d) {
        for (size_t l = 0; l < WORDS_PER_DOC; ++l) {
            assert(rebuilt[d][l] == corpus.doc_words[d][l]);
        }
    }
    assert(total_posts == total_docs * WORDS_PER_DOC);
}


void test_loaded_index_matches_corpus() {
    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");

    std::mt19937_64 rng(RNG_SEED);
    vector<string> pool = make_word_pool();
    Corpus corpus = generate_corpus(pool, rng);

    write_parser_files(pool, corpus);
    string chunk_path = build_index_chunk();
    verify_loaded_index(chunk_path, pool, corpus);

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
}


// Intersect word_to_docs[pidx] across every query word via two-pointer sweep.
static std::set<uint32_t> expected_docs(const Corpus& corpus,
                                        const vector<uint32_t>& pool_idxs) {
    std::set<uint32_t> out;
    if (pool_idxs.size() == 0) return out;
    out = corpus.word_to_docs[pool_idxs[0]];
    for (size_t i = 1; i < pool_idxs.size(); ++i) {
        const std::set<uint32_t>& other = corpus.word_to_docs[pool_idxs[i]];
        std::set<uint32_t> next;
        auto a = out.begin();
        auto b = other.begin();
        while (a != out.end() && b != other.end()) {
            if (*a < *b)      ++a;
            else if (*b < *a) ++b;
            else { next.insert(*a); ++a; ++b; }
        }
        out = next;
    }
    return out;
}


// Map a corpus URL back to its 1-indexed doc id by linear scan. Used to
// translate DocInfo.url into a doc id so we can compare results against the
// existing expected_docs() ground truth.
static uint32_t doc_id_for_url(const Corpus& corpus, const string& url) {
    for (size_t d = 0; d < corpus.urls.size(); ++d) {
        if (corpus.urls[d].size() == url.size() &&
            memcmp(corpus.urls[d].data(), url.data(), url.size()) == 0) {
            return static_cast<uint32_t>(d + 1);
        }
    }
    return 0;
}


// Find a word string's pool index by linear scan, so we can look up its
// corpus-side expected positions.
static uint32_t pool_idx_for_word(const vector<string>& pool, const string& w) {
    for (size_t k = 0; k < pool.size(); ++k) {
        if (pool[k].size() == w.size() &&
            memcmp(pool[k].data(), w.data(), w.size()) == 0) {
            return static_cast<uint32_t>(k);
        }
    }
    return UINT32_MAX;
}


void test_chunk_manager_queries() {
    constexpr size_t NUM_QUERIES = 200;

    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");

    std::mt19937_64 rng(RNG_SEED ^ 0xA5A5A5A5ULL);
    vector<string> pool = make_word_pool();
    Corpus corpus = generate_corpus(pool, rng);

    write_parser_files(pool, corpus);
    string chunk_path = build_index_chunk();

    // chunk_manager now pushes matches into a caller-owned atomic_vector passed
    // per-call. Drain the channel after each query via take().
    atomic_vector<DocInfo> collector;
    chunk_manager cm(chunk_path);

    // Bump the per-call deadline so small-corpus leapfrog never hits the
    // production 250ms cap and returns a partial result that wouldn't match
    // our ground truth.
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

        // Sample without replacement
        std::set<uint32_t> seen;
        while (pool_idxs.size() < k) {
            uint32_t p = pdist(rng);
            if (seen.insert(p).second) {
                pool_idxs.push_back(p);
                words.push_back(string(pool[p].data(), pool[p].size()));
            }
        }

        std::set<uint32_t> want = expected_docs(corpus, pool_idxs);
        cm.default_query(words, &collector);
        vector<DocInfo> got = collector.take();

        assert(got.size() == want.size());

        // Translate each DocInfo back to a doc id via its URL, and check that
        // the set matches expected_docs and the ordering is strictly ascending
        // (leapfrog emits in doc-id order even though DocInfo carries urls).
        vector<uint32_t> got_ids;
        got_ids.reserve(got.size());
        for (const DocInfo& di : got) {
            uint32_t doc_id = doc_id_for_url(corpus, di.url);
            assert(doc_id != 0);
            got_ids.push_back(doc_id);
        }
        for (size_t i = 1; i < got_ids.size(); ++i) {
            assert(got_ids[i] > got_ids[i - 1]);
        }
        size_t j = 0;
        for (uint32_t d : want) {
            assert(got_ids[j] == d);
            ++j;
        }

        // For each returned DocInfo, verify that collect_positions_in_current_doc
        // yielded exactly the positions the corpus recorded for that word in
        // that doc. Positions on disk are 1-indexed (l_idx + 1 in the indexer),
        // so we compare against the same shift here.
        for (size_t di_i = 0; di_i < got.size(); ++di_i) {
            const DocInfo& di = got[di_i];
            const uint32_t doc_id = got_ids[di_i];
            const size_t g = static_cast<size_t>(doc_id) - 1;

            assert(di.wordInfo.size() == words.size());
            for (const WordInfo& wi : di.wordInfo) {
                uint32_t pidx = pool_idx_for_word(pool, wi.word);
                assert(pidx != UINT32_MAX);

                std::set<size_t> expected_positions;
                const vector<uint32_t>& row = corpus.doc_words[g];
                for (size_t l = 0; l < row.size(); ++l) {
                    if (row[l] == pidx) expected_positions.insert(l + 1);
                }

                assert(wi.pos.size() == expected_positions.size());
                std::set<size_t> got_positions;
                for (size_t p : wi.pos) got_positions.insert(p);
                assert(got_positions == expected_positions);
            }
        }

        if (want.empty()) empty++;
        else              nonempty++;
    }

    // Both codepaths must be exercised: sparse per-doc subsets give us plenty
    // of empty intersections, and single-word queries guarantee hits.
    assert(nonempty > 0);
    assert(empty    > 0);

    // Words not in the dictionary must short-circuit to an empty result.
    vector<string> only_missing;
    only_missing.push_back(string("nothing"));
    cm.default_query(only_missing, &collector);
    assert(collector.take().size() == 0);

    vector<string> mixed;
    mixed.push_back(string(pool[0].data(), pool[0].size()));
    mixed.push_back(string("nothing"));
    cm.default_query(mixed, &collector);
    assert(collector.take().size() == 0);

    printf("  queries: %zu non-empty, %zu empty\n", nonempty, empty);

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
}

} // namespace


int main() {
    printf("\n=== RUNNING INDEX UTIL TESTS ===\n\n");

    test_loaded_index_matches_corpus();
    test_chunk_manager_queries();

    printf("\n=== ALL INDEX UTIL TESTS FINISHED ===\n\n");
    return 0;
}
