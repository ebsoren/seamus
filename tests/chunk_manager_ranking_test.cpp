#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

#include "index/Index.h"
#include "index_chunk/chunk_manager.h"
#include "lib/atomic_vector.h"
#include "lib/consts.h"
#include "lib/chunk_manager_query.h"
#include "lib/rpc_query_handler.h"
#include "lib/string.h"
#include "lib/vector.h"
#include "query/expressions.h"
#include "ranker/Ranker.h"


namespace {

constexpr const char* PARSER_DIR = "/tmp/cm_ranking_test";


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


// -----------------------------------------------------------------
// Corpus: 7 docs with controlled word sequences (same as query_isr_test).
// -----------------------------------------------------------------

struct DocDef {
    const char* url;
    const char* words[6];
    size_t n_words;
};

static const DocDef DOCS[] = {
    {"http://t/1", {"apple", "banana", "cherry", "date",  "elder", "fig"},   6},
    {"http://t/2", {"banana","cherry", "grape",  "honey", "iris",  "jade"},  6},
    {"http://t/3", {"apple", "cherry", "elder",  "grape", "kiwi",  "lemon"}, 6},
    {"http://t/4", {"cherry","fig",    "grape",  "honey", "iris",  "date"},  6},
    {"http://t/5", {"apple", "date",   "banana", "fig",   "grape", "honey"}, 6},
    {"http://t/6", {"apple", "banana", "cherry", "fig",   "elder", "grape"}, 6},
    {"http://t/7", {"apple", "kiwi",   "date",   "grape", "honey", "iris"},  6},
};
static constexpr size_t NUM_DOCS = sizeof(DOCS) / sizeof(DOCS[0]);


static string parser_file_path() {
    return string::join("", string(PARSER_DIR), "/docs_0.txt");
}


static void write_corpus() {
    mkdir_p(PARSER_DIR);
    string path = parser_file_path();
    FILE* fd = fopen(path.data(), "w");
    assert(fd != nullptr);
    for (size_t d = 0; d < NUM_DOCS; ++d) {
        fputs("<doc>\n", fd);
        fputs(DOCS[d].url, fd);
        fputc('\n', fd);
        for (size_t w = 0; w < DOCS[d].n_words; ++w) {
            fputs(DOCS[d].words[w], fd);
            fputc('\n', fd);
        }
        fputs("</doc>\n", fd);
    }
    fclose(fd);
}


static string build_chunk() {
    IndexChunk idx(0);
    assert(idx.index_file(parser_file_path()));
    idx.flush();
    return string::join("", string(INDEX_OUTPUT_DIR), "/index_chunk_0_0.txt");
}


// Helper: collect LeanPage URLs from a chunk_manager query.
static std::set<std::string> query_urls(chunk_manager& cm, const char* query_str) {
    string qs(query_str);
    ASTNode ast = parse_query_ast(string_view(qs.data(), qs.size()));

    atomic_vector<LeanPage> collector;
    cm.query(ast, &collector);
    vector<LeanPage> pages = collector.take();

    std::set<std::string> urls;
    for (size_t i = 0; i < pages.size(); ++i) {
        urls.insert(std::string(pages[i].url.data(), pages[i].url.size()));
    }
    return urls;
}


// Helper: run a query and return the LeanPages.
static vector<LeanPage> query_pages(chunk_manager& cm, const char* query_str) {
    string qs(query_str);
    ASTNode ast = parse_query_ast(string_view(qs.data(), qs.size()));

    atomic_vector<LeanPage> collector;
    cm.query(ast, &collector);
    return collector.take();
}


static std::set<std::string> make_url_set(std::initializer_list<const char*> urls) {
    std::set<std::string> s;
    for (auto u : urls) s.insert(u);
    return s;
}


static void assert_urls(const char* query_str, chunk_manager& cm,
                        std::set<std::string> expected) {
    std::set<std::string> got = query_urls(cm, query_str);
    if (got != expected) {
        printf("FAIL: query \"%s\"\n  expected %zu url(s):", query_str, expected.size());
        for (auto& u : expected) printf(" %s", u.c_str());
        printf("\n  got      %zu url(s):", got.size());
        for (auto& u : got) printf(" %s", u.c_str());
        printf("\n");
    }
    assert(got == expected);
}


// =================================================================
// Test 1: chunk_manager::query() returns correct URLs (no UrlStore)
//
// NOTE: chunk_manager::query() currently extracts positive terms
// and delegates to default_query (leapfrog AND).  OR, NOT, and
// phrase semantics are not yet executed — those need ISR-tree-based
// execution (tested separately in query_isr_test).  This test
// covers the queries that the current implementation handles.
// =================================================================

void test_chunk_manager_query_urls() {
    printf("  chunk_manager query URL correctness...\n");
    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
    write_corpus();
    string chunk_path = build_chunk();

    chunk_manager cm(chunk_path);

    // Single term
    assert_urls("apple", cm, make_url_set({"http://t/1","http://t/3","http://t/5","http://t/6","http://t/7"}));
    assert_urls("jade", cm, make_url_set({"http://t/2"}));

    // AND (2-way)
    assert_urls("apple banana", cm, make_url_set({"http://t/1","http://t/5","http://t/6"}));

    // AND (3-way)
    assert_urls("apple cherry grape", cm, make_url_set({"http://t/3","http://t/6"}));

    // AND with no overlap
    assert_urls("jade lemon", cm, make_url_set({}));

    // Nonexistent term
    assert_urls("zzzzz", cm, make_url_set({}));

    // AND with nonexistent term → empty
    assert_urls("apple zzzzz", cm, make_url_set({}));

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
}


// =================================================================
// Test 2: Without UrlStore, all scores should be 0.0
// =================================================================

void test_chunk_manager_no_urlstore_scores() {
    printf("  chunk_manager no-UrlStore scores are 0.0...\n");
    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
    write_corpus();
    string chunk_path = build_chunk();

    chunk_manager cm(chunk_path);

    vector<LeanPage> pages = query_pages(cm, "apple banana");
    assert(pages.size() == 3);
    for (size_t i = 0; i < pages.size(); ++i) {
        assert(pages[i].score == 0.0);
    }

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
}


// =================================================================
// Test 3: Ranker static score — well-formed URLs score higher
// =================================================================

void test_static_score_url_quality() {
    printf("  Ranker static score: URL quality ordering...\n");

    auto make_page = [](const char* url) -> RankedPage {
        RankedPage p;
        p.url = string(url);
        p.title = string("");
        p.seed_list_dist = 0;
        p.domains_from_seed = 0;
        p.num_unique_words_found_anchor = 0;
        p.num_unique_words_found_title = 0;
        p.num_unique_words_found_url = 0;
        p.times_seen = 1;
        p.doc_len = 100;
        return p;
    };

    // .edu TLD should score higher than obscure TLD
    RankedPage edu_page = make_page("https://example.edu/article");
    RankedPage xyz_page = make_page("https://example.xyz/article");
    double edu_score = calc_static_score(edu_page);
    double xyz_score = calc_static_score(xyz_page);
    assert(edu_score > xyz_score);

    // Short domain beats long domain
    RankedPage short_page = make_page("https://ex.com/a");
    RankedPage long_page = make_page("https://very-long-domain-name-here.com/a");
    double short_score = calc_static_score(short_page);
    double long_score = calc_static_score(long_page);
    assert(short_score > long_score);

    // Shallow path beats deep path
    RankedPage shallow = make_page("https://example.com/page");
    RankedPage deep = make_page("https://example.com/a/b/c/d/e/f/page");
    double shallow_score = calc_static_score(shallow);
    double deep_score = calc_static_score(deep);
    assert(shallow_score > deep_score);

    // No query string beats query string
    RankedPage clean = make_page("https://example.com/page");
    RankedPage messy = make_page("https://example.com/page?id=123&ref=456");
    double clean_score = calc_static_score(clean);
    double messy_score = calc_static_score(messy);
    assert(clean_score > messy_score);

    // Fewer subdomains is better
    RankedPage simple = make_page("https://example.com/page");
    RankedPage subbed = make_page("https://a.b.c.example.com/page");
    double simple_score = calc_static_score(simple);
    double subbed_score = calc_static_score(subbed);
    assert(simple_score > subbed_score);

    // Digits in domain hurt
    RankedPage alpha = make_page("https://example.com/page");
    RankedPage digits = make_page("https://ex4mpl3.com/page");
    double alpha_score = calc_static_score(alpha);
    double digits_score = calc_static_score(digits);
    assert(alpha_score > digits_score);

    printf("    all static score orderings correct\n");
}


// =================================================================
// Test 4: word_pos_score — proximity and frequency effects
// =================================================================

void test_word_pos_score() {
    printf("  Ranker word_pos_score: proximity and frequency...\n");

    // More occurrences → higher score
    {
        vector<vector<size_t>> few;
        few.push_back(vector<size_t>());
        few[0].push_back(10);

        vector<vector<size_t>> many;
        many.push_back(vector<size_t>());
        many[0].push_back(10);
        many[0].push_back(20);
        many[0].push_back(30);
        many[0].push_back(40);

        double few_score = word_pos_score(few, 100, 1);
        double many_score = word_pos_score(many, 100, 1);
        assert(many_score > few_score);
    }

    // Close proximity → higher score (two words)
    {
        vector<vector<size_t>> close;
        close.push_back(vector<size_t>());
        close[0].push_back(10);
        close.push_back(vector<size_t>());
        close[1].push_back(11);

        vector<vector<size_t>> far;
        far.push_back(vector<size_t>());
        far[0].push_back(10);
        far.push_back(vector<size_t>());
        far[1].push_back(500);

        double close_score = word_pos_score(close, 1000, 2);
        double far_score = word_pos_score(far, 1000, 2);
        assert(close_score > far_score);
    }

    // Zero words or zero doc_len → 0.0
    {
        vector<vector<size_t>> empty;
        assert(word_pos_score(empty, 100, 0) == 0.0);
        assert(word_pos_score(empty, 0, 1) == 0.0);
    }

    printf("    all word_pos_score orderings correct\n");
}


} // namespace


int main() {
    printf("\n=== RUNNING CHUNK MANAGER RANKING TESTS ===\n\n");

    test_chunk_manager_query_urls();
    test_chunk_manager_no_urlstore_scores();
    test_static_score_url_quality();
    test_word_pos_score();

    printf("\n=== ALL CHUNK MANAGER RANKING TESTS PASSED ===\n\n");
    return 0;
}
