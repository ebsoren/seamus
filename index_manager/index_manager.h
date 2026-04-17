#pragma once

#include <atomic>
#include <cstdint>

#include "index_chunk/chunk_manager.h"
#include "lib/atomic_vector.h"
#include "lib/consts.h"
#include "lib/joinable_thread_pool.h"
#include "lib/logger.h"
#include "lib/rpc_query_handler.h"
#include "lib/string.h"
#include "lib/utils.h"
#include "lib/vector.h"
#include "query/expressions.h"


class IndexManager {
public:
    // Pool is shared across every handle_query call — workers stay alive
    // so we pay the pthread_create cost once at startup instead of per
    // query. 300 lets a single fan-out overlap dozens of chunk queries
    // while leaving headroom for I/O-heavy ISR scans.
    static constexpr size_t POOL_THREADS = 10;

    // Walk every worker's chunk files in order; stop at the first missing
    // chunk for each worker. Mirrors recover_index_chunks, which targets
    // IndexChunk::get_index_chunk_path's naming.
    explicit IndexManager(UrlStore *url_store = nullptr) : pool(POOL_THREADS) {
        for (uint32_t w = 0; w < NUM_INDEXER_THREADS; ++w) {
            for (uint32_t c = 0;; ++c) {
                string path = string::join("", string(INDEX_OUTPUT_DIR), "/index_chunk_", string(w), "_",
                                           string(c), ".txt");
                if (!file_exists(path)) break;
                fprintf(stderr, "[INDEX_MANAGER] loaded chunk: %.*s\n",
                        static_cast<int>(path.size()), path.data());
                chunk_managers.emplace_back(path, url_store);
            }
        }
        fprintf(stderr, "[INDEX_MANAGER] init complete: %zu chunks loaded from %s\n",
                chunk_managers.size(), INDEX_OUTPUT_DIR);
        fflush(stderr);
    }

    IndexManager(const IndexManager &)            = delete;
    IndexManager &operator=(const IndexManager &) = delete;

    // Parse the raw query string once, then fan the resulting AST out to
    // every chunk's tree-based executor via the shared pool. Each chunk
    // builds its own ISR tree off the shared AST inside
    // chunk_manager::query — the AST itself is immutable across chunks
    // so the read-only share is safe. Blocks in pool.join() until every
    // submitted task has finished, so the lambda captures stay valid.
    //
    // Each call gets a unique query ID so concurrent handle_query calls
    // only join on their own fan-out, not on each other's tasks.
    LeanPageResponse handle_query(const string &query_str) {
        fprintf(stderr, "[INDEX_MANAGER] handle_query query='%.*s' num_chunks=%zu\n",
                static_cast<int>(query_str.size()), query_str.data(), chunk_managers.size());
        fflush(stderr);

        LeanPageResponse resp;

        ASTNode ast;
        try {
            ast = parse_query_ast(string_view(query_str.data(), query_str.size()));
        } catch (const ParseError &e) {
            fprintf(stderr, "[INDEX_MANAGER] parse failed: %s\n", e.message);
            fflush(stderr);
            return resp;
        }

        fprintf(stderr, "[INDEX_MANAGER] AST parsed OK, fanning out to %zu chunks\n", chunk_managers.size());
        fflush(stderr);

        atomic_vector<LeanPage> collector;

        Ranker::reset_stats();

        int qid = next_query_id.fetch_add(1);

        for (chunk_manager &cm : chunk_managers) {
            pool.submit(qid, [&cm, &ast, &collector]() {
                cm.query(ast, &collector);
            });
        }
        pool.join(qid);

        Ranker::print_stats();

        resp.pages = collector.take();
        fprintf(stderr, "[INDEX_MANAGER] query complete, collected %zu pages\n", resp.pages.size());
        fflush(stderr);
        return resp;
    }

private:
    JoinableThreadPool pool;
    std::atomic<int> next_query_id{0};
    vector<chunk_manager> chunk_managers;
};
