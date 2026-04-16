#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

#include "index/Index.h"
#include "index_chunk/chunk_manager.h"
#include "lib/consts.h"
#include "lib/string.h"
#include "lib/vector.h"
#include "query/expressions.h"


namespace {

constexpr const char* PARSER_DIR = "/tmp/query_isr_test";


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
// Corpus: 7 docs with controlled word sequences.
//
//   Doc 1: apple banana cherry date elder fig
//   Doc 2: banana cherry grape honey iris jade
//   Doc 3: apple cherry elder grape kiwi lemon
//   Doc 4: cherry fig grape honey iris date
//   Doc 5: apple date banana fig grape honey
//   Doc 6: apple banana cherry fig elder grape
//   Doc 7: apple kiwi date grape honey iris
//
// Word presence by doc:
//   apple:  {1, 3, 5, 6, 7}
//   banana: {1, 2, 5, 6}
//   cherry: {1, 2, 3, 4, 6}
//   date:   {1, 4, 5, 7}
//   elder:  {1, 3, 6}
//   fig:    {1, 4, 5, 6}
//   grape:  {2, 3, 4, 5, 6, 7}
//   honey:  {2, 4, 5, 7}
//   iris:   {2, 4, 7}
//   jade:   {2}
//   kiwi:   {3, 7}
//   lemon:  {3}
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


// Run a query through the ISR tree and collect all matching 1-indexed doc ids.
static std::set<uint32_t> run_query(const char* query_str, LoadedIndex* li) {
    string qs(query_str);
    ASTNode ast = parse_query_ast(string_view(qs.data(), qs.size()));

    ISRArena arena;
    QueryISR* root = build_isr_tree(ast, arena, li);
    assert(root != nullptr);

    std::set<uint32_t> result;
    if (!root->is_driveable()) return result;

    uint32_t doc = root->next_doc();
    while (doc != 0) {
        result.insert(doc);
        doc = root->next_doc();
    }
    return result;
}


static std::set<uint32_t> make_set(std::initializer_list<uint32_t> vals) {
    return std::set<uint32_t>(vals);
}


static void assert_query(const char* query_str, LoadedIndex* li,
                         std::set<uint32_t> expected) {
    std::set<uint32_t> got = run_query(query_str, li);
    if (got != expected) {
        printf("FAIL: query \"%s\"\n  expected {", query_str);
        for (uint32_t d : expected) printf(" %u", d);
        printf(" }\n  got      {");
        for (uint32_t d : got) printf(" %u", d);
        printf(" }\n");
    }
    assert(got == expected);
}


// -----------------------------------------------------------------
// Tests
// -----------------------------------------------------------------

void test_single_term() {
    printf("  single term queries...\n");
    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
    write_corpus();
    string chunk_path = build_chunk();
    LoadedIndex li(chunk_path);

    assert_query("apple",  &li, make_set({1, 3, 5, 6, 7}));
    assert_query("banana", &li, make_set({1, 2, 5, 6}));
    assert_query("iris",   &li, make_set({2, 4, 7}));
    assert_query("jade",   &li, make_set({2}));
    assert_query("lemon",  &li, make_set({3}));

    // Nonexistent word → empty
    assert_query("zzzzz", &li, make_set({}));

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
}


void test_and_queries() {
    printf("  AND queries...\n");
    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
    write_corpus();
    string chunk_path = build_chunk();
    LoadedIndex li(chunk_path);

    // apple={1,3,5,6,7}  banana={1,2,5,6}
    assert_query("apple banana", &li, make_set({1, 5, 6}));

    // cherry={1,2,3,4,6}  elder={1,3,6}
    assert_query("cherry elder", &li, make_set({1, 3, 6}));

    // apple={1,3,5,6,7}  iris={2,4,7}
    assert_query("apple iris", &li, make_set({7}));

    // Three-way AND: apple={1,3,5,6,7}  cherry={1,2,3,4,6}  grape={2,3,4,5,6,7}
    assert_query("apple cherry grape", &li, make_set({3, 6}));

    // Real word AND nonexistent → empty
    assert_query("apple zzzzz", &li, make_set({}));

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
}


void test_or_queries() {
    printf("  OR queries...\n");
    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
    write_corpus();
    string chunk_path = build_chunk();
    LoadedIndex li(chunk_path);

    // jade={2}  lemon={3}
    assert_query("jade | lemon", &li, make_set({2, 3}));

    // iris={2,4,7}  elder={1,3,6}
    assert_query("iris | elder", &li, make_set({1, 2, 3, 4, 6, 7}));

    // honey={2,4,5,7}  banana={1,2,5,6}
    assert_query("honey | banana", &li, make_set({1, 2, 4, 5, 6, 7}));

    // Three-way OR: jade={2}  lemon={3}  iris={2,4,7}
    assert_query("jade | lemon | iris", &li, make_set({2, 3, 4, 7}));

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
}


void test_not_queries() {
    printf("  NOT queries...\n");
    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
    write_corpus();
    string chunk_path = build_chunk();
    LoadedIndex li(chunk_path);

    // apple={1,3,5,6,7}  NOT banana={1,2,5,6}  →  {3, 7}
    assert_query("apple -banana", &li, make_set({3, 7}));

    // cherry={1,2,3,4,6}  NOT apple={1,3,5,6,7}  →  {2, 4}
    assert_query("cherry -apple", &li, make_set({2, 4}));

    // apple AND banana AND NOT cherry: {1,5,6} minus cherry={1,2,3,4,6}  →  {5}
    assert_query("apple banana -cherry", &li, make_set({5}));

    // Bare negation: not driveable
    {
        string qs("-apple");
        ASTNode ast = parse_query_ast(string_view(qs.data(), qs.size()));
        ISRArena arena;
        QueryISR* root = build_isr_tree(ast, arena, &li);
        assert(root != nullptr);
        assert(!root->is_driveable());
    }

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
}


void test_phrase_queries() {
    printf("  phrase queries...\n");
    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
    write_corpus();
    string chunk_path = build_chunk();
    LoadedIndex li(chunk_path);

    // "apple banana" as phrase (adjacent):
    //   Doc 1: apple(1) banana(2) → adjacent ✓
    //   Doc 5: apple(1) date(2) banana(3) → NOT adjacent
    //   Doc 6: apple(1) banana(2) → adjacent ✓
    assert_query("\"apple banana\"", &li, make_set({1, 6}));

    // "banana cherry" as phrase:
    //   Doc 1: banana(2) cherry(3) → ✓
    //   Doc 2: banana(1) cherry(2) → ✓
    //   Doc 6: banana(2) cherry(3) → ✓
    assert_query("\"banana cherry\"", &li, make_set({1, 2, 6}));

    // "cherry fig" as phrase:
    //   Doc 1: cherry(3) ... fig(6) → NOT adjacent
    //   Doc 4: cherry(1) fig(2) → ✓
    //   Doc 6: cherry(3) fig(4) → ✓
    assert_query("\"cherry fig\"", &li, make_set({4, 6}));

    // Three-word phrase "apple banana cherry":
    //   Doc 1: apple(1) banana(2) cherry(3) → ✓
    //   Doc 6: apple(1) banana(2) cherry(3) → ✓
    assert_query("\"apple banana cherry\"", &li, make_set({1, 6}));

    // Single-word "phrase" — same as a plain term
    assert_query("\"apple\"", &li, make_set({1, 3, 5, 6, 7}));

    // Nonexistent phrase
    assert_query("\"jade lemon\"", &li, make_set({}));

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
}


void test_complex_queries() {
    printf("  complex queries...\n");
    clean_dir(PARSER_DIR, nullptr);
    mkdir_p(INDEX_OUTPUT_DIR);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
    write_corpus();
    string chunk_path = build_chunk();
    LoadedIndex li(chunk_path);

    // (apple | iris) AND cherry
    //   apple|iris = {1,2,3,4,5,6,7}  cherry={1,2,3,4,6}
    //   → {1, 2, 3, 4, 6}
    assert_query("(apple | iris) cherry", &li, make_set({1, 2, 3, 4, 6}));

    // (jade | kiwi) AND grape
    //   jade|kiwi = {2,3,7}  grape={2,3,4,5,6,7}
    //   → {2, 3, 7}
    assert_query("(jade | kiwi) grape", &li, make_set({2, 3, 7}));

    // (apple | iris) AND (fig | elder)
    //   apple|iris = {1,2,3,4,5,6,7}
    //   fig|elder  = {1,3,4,5,6}
    //   → {1, 3, 4, 5, 6}
    assert_query("(apple | iris) (fig | elder)", &li, make_set({1, 3, 4, 5, 6}));

    // Phrase AND term: "apple banana" AND cherry
    //   phrase "apple banana" = {1, 6}  cherry={1,2,3,4,6}
    //   → {1, 6}
    assert_query("\"apple banana\" cherry", &li, make_set({1, 6}));

    // Phrase AND NOT: "banana cherry" AND NOT apple
    //   phrase "banana cherry" = {1, 2, 6}  NOT apple → not in {1,3,5,6,7}
    //   → {2}
    assert_query("\"banana cherry\" -apple", &li, make_set({2}));

    // OR of phrases: "cherry fig" | "apple banana"
    //   phrase "cherry fig" = {4, 6}  phrase "apple banana" = {1, 6}
    //   → {1, 4, 6}
    assert_query("\"cherry fig\" | \"apple banana\"", &li, make_set({1, 4, 6}));

    clean_dir(PARSER_DIR, nullptr);
    clean_dir(INDEX_OUTPUT_DIR, "index_chunk_0_");
}

} // namespace


int main() {
    printf("\n=== RUNNING QUERY ISR TESTS ===\n\n");

    test_single_term();
    test_and_queries();
    test_or_queries();
    test_not_queries();
    test_phrase_queries();
    test_complex_queries();

    printf("\n=== ALL QUERY ISR TESTS PASSED ===\n\n");
    return 0;
}
