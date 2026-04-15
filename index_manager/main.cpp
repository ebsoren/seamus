// Tiny CLI for smoke-testing index_manager against a real on-disk index.
// Usage: index_manager_cli <word1> [word2 ...]
//
// Reads chunks from INDEX_OUTPUT_DIR (see lib/consts.h), runs a leapfrog AND
// across the given words via index_manager::handle_query, and prints the
// matching URLs plus per-word positions. Words must be unique — that's a
// precondition of default_query and the CLI doesn't dedupe for you.

#include <cstdio>

#include "index_manager/index_manager.h"
#include "lib/consts.h"
#include "lib/query_response.h"
#include "lib/string.h"
#include "lib/vector.h"


int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <word> [word ...]\n", argv[0]);
        fprintf(stderr, "  runs an AND query across index chunks in %s\n", INDEX_OUTPUT_DIR);
        return 1;
    }

    vector<string> words;
    words.reserve(static_cast<size_t>(argc - 1));
    for (int i = 1; i < argc; ++i) {
        words.push_back(string(argv[i]));
    }

    printf("loading index from %s...\n", INDEX_OUTPUT_DIR);
    index_manager im;

    printf("query: ");
    for (size_t i = 0; i < words.size(); ++i) {
        if (i > 0) printf(" AND ");
        printf("%s", words[i].data());
    }
    printf("\n");

    QueryResponse resp = im.handle_query(words);

    printf("%zu matching doc(s)\n", resp.pages.size());
    for (size_t i = 0; i < resp.pages.size(); ++i) {
        const DocInfo& di = resp.pages[i];
        printf("  [%zu] %.*s\n", i, static_cast<int>(di.url.size()), di.url.data());
        for (size_t w = 0; w < di.wordInfo.size(); ++w) {
            const WordInfo& wi = di.wordInfo[w];
            printf("      %.*s (%zu hit%s):",
                   static_cast<int>(wi.word.size()), wi.word.data(),
                   wi.pos.size(), wi.pos.size() == 1 ? "" : "s");
            for (size_t p = 0; p < wi.pos.size(); ++p) {
                printf(" %zu", wi.pos[p]);
            }
            printf("\n");
        }
    }

    return 0;
}
