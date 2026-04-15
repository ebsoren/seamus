#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <random>
#include <sys/stat.h>
#include <unistd.h>

#include "index/Index.h"
#include "index_chunk/chunk_manager.h"
#include "lib/consts.h"
#include "lib/string.h"
#include "lib/vector.h"


namespace {

constexpr const char* PARSER_DIR     = "/tmp/index_util_test";
constexpr size_t      NUM_FILES      = 3;
constexpr size_t      DOCS_PER_FILE  = 5;
constexpr size_t      WORDS_PER_DOC  = 200;
constexpr uint64_t    RNG_SEED       = 0xC0FFEEULL;


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
    vector<string>           urls;       // urls[g] for global doc g
    vector<vector<uint32_t>> doc_words;  // doc_words[g][l] = pool index
};


static Corpus generate_corpus(const vector<string>& pool, std::mt19937_64& rng) {
    Corpus c;
    const size_t total_docs = NUM_FILES * DOCS_PER_FILE;
    c.urls.reserve(total_docs);
    c.doc_words.reserve(total_docs);

    std::uniform_int_distribution<uint32_t> wd(0, pool.size() - 1);

    char url_buf[64];
    for (size_t f = 0; f < NUM_FILES; ++f) {
        for (size_t d = 0; d < DOCS_PER_FILE; ++d) {
            int n = snprintf(url_buf, sizeof(url_buf),
                             "http://test.local/f%zu/d%zu", f, d);
            c.urls.push_back(string(url_buf, static_cast<size_t>(n)));

            vector<uint32_t> row;
            row.reserve(WORDS_PER_DOC);
            for (size_t w = 0; w < WORDS_PER_DOC; ++w) {
                row.push_back(wd(rng));
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

} // namespace


int main() {
    printf("\n=== RUNNING INDEX UTIL TESTS ===\n\n");

    test_loaded_index_matches_corpus();

    printf("\n=== ALL INDEX UTIL TESTS FINISHED ===\n\n");
    return 0;
}
