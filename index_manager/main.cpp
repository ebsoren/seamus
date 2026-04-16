// REPL CLI for smoke-testing index_manager against a real on-disk index.
// Loads every chunk under INDEX_OUTPUT_DIR once, then reads queries from
// stdin one line at a time. Words on a line are the (unique) terms for an
// AND query. Blank line, "quit", "exit", or EOF terminates.
//
// Usage:
//   bazel run //index_manager:index_manager_cli
//   <enter queries interactively>
//
// Or pipe a file of queries:
//   echo "hello world" | bazel run //index_manager:index_manager_cli

#include <chrono>
#include <cstdio>
#include <cstring>

#include "index_manager/index_manager.h"
#include "lib/consts.h"
#include "lib/chunk_manager_query.h"
#include "lib/string.h"


namespace {

static void print_response(const LeanPageResponse& resp) {
    printf("  %zu matching doc(s)\n", resp.pages.size());
    for (size_t i = 0; i < resp.pages.size(); ++i) {
        const LeanPage& lp = resp.pages[i];
        printf("    [%zu] %.*s  (score %.4f)\n",
               i, static_cast<int>(lp.url.size()), lp.url.data(), lp.score);
    }
}

} // namespace


int main(int /*argc*/, char* /*argv*/[]) {
    using clock = std::chrono::steady_clock;

    printf("loading index from %s ...\n", INDEX_OUTPUT_DIR);
    auto load_start = clock::now();
    IndexManager im;
    auto load_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - load_start).count();
    printf("index loaded in %lld ms\n", static_cast<long long>(load_ms));
    printf("enter queries (one per line, blank/'quit'/Ctrl-D to exit):\n");
    fflush(stdout);

    char line[4096];
    while (true) {
        printf("> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) break;
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) break;

        string query_str(line, len);

        auto q_start = clock::now();
        LeanPageResponse resp = im.handle_query(query_str);
        auto q_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - q_start).count();

        print_response(resp);
        printf("  (%lld ms)\n", static_cast<long long>(q_ms));
        fflush(stdout);
    }

    return 0;
}
