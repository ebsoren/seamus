#pragma once

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
    static constexpr size_t POOL_THREADS = 300;

    // Walk every worker's chunk files in order; stop at the first missing
    // chunk for each worker. Mirrors recover_index_chunks, which targets
    // IndexChunk::get_index_chunk_path's naming.
    IndexManager() : pool(POOL_THREADS) {
        for (uint32_t w = 0; w < NUM_INDEXER_THREADS; ++w) {
            for (uint32_t c = 0; c < 5; ++c) {
                string path = string::join("", string(INDEX_OUTPUT_DIR), "/index_chunk_", string(w), "_",
                                           string(c), ".txt");
                if (!file_exists(path)) break;
                chunk_managers.emplace_back(path);
            }
        }
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
    // Assumes a single-threaded dispatcher: pool.join() waits for every
    // currently pending task, not just the tasks this call submitted.
    LeanPageResponse handle_query(const string &query_str) {
        LeanPageResponse resp;

        ASTNode ast;
        try {
            ast = parse_query_ast(string_view(query_str.data(), query_str.size()));
        } catch (const ParseError &e) {
            logger::warn("index_manager::handle_query: parse failed: %s", e.message);
            return resp;
        }

        atomic_vector<LeanPage> collector;

        for (chunk_manager &cm : chunk_managers) {
            pool.submit([&cm, &ast, &collector]() {
                cm.query(ast, &collector);
            });
        }
        pool.join();

        resp.pages = collector.take();
        return resp;
    }

private:
    JoinableThreadPool pool;
    vector<chunk_manager> chunk_managers;
};
