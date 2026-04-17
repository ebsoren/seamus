#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

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
        // 1. Discover all chunk paths (fast, single-threaded)
        vector<string> paths;
        for (uint32_t w = 0; w < NUM_INDEXER_THREADS; ++w) {
            for (uint32_t c = 0;; ++c) {
                string path = string::join("", string(INDEX_OUTPUT_DIR), "/index_chunk_", string(w), "_",
                                           string(c), ".txt");
                if (!file_exists(path)) break;
                paths.push_back(std::move(path));
            }
        }

        size_t n = paths.size();
        logger::error("[INDEX_MANAGER] found %zu chunks, loading with %zu threads",
                n, (size_t)NUM_INDEXER_THREADS);

        // 2. Load chunks in parallel — each fread is independent
        std::mutex mtx;
        std::atomic<size_t> next{0};

        auto loader = [&]() {
            while (true) {
                size_t i = next.fetch_add(1, std::memory_order_relaxed);
                if (i >= n) break;
                chunk_manager cm(paths[i], url_store);
                logger::error("[INDEX_MANAGER] loaded chunk [%zu/%zu]: %.*s",
                        i + 1, n, static_cast<int>(paths[i].size()), paths[i].data());
                std::lock_guard<std::mutex> lock(mtx);
                chunk_managers.push_back(std::move(cm));
            }
        };

        size_t num_loaders = n < NUM_INDEXER_THREADS ? n : NUM_INDEXER_THREADS;
        std::vector<std::thread> threads;
        for (size_t t = 0; t < num_loaders; ++t)
            threads.emplace_back(loader);
        for (auto& t : threads) t.join();

        logger::error("[INDEX_MANAGER] init complete: %zu chunks loaded from %s",
                chunk_managers.size(), INDEX_OUTPUT_DIR);
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
        logger::warn("[INDEX_MANAGER] handle_query query='%.*s' num_chunks=%zu",
                static_cast<int>(query_str.size()), query_str.data(), chunk_managers.size());

        LeanPageResponse resp;

        ASTNode ast;
        try {
            ast = parse_query_ast(string_view(query_str.data(), query_str.size()));
        } catch (const ParseError &e) {
            logger::warn("[INDEX_MANAGER] parse failed: %s", e.message);
            return resp;
        }

        logger::warn("[INDEX_MANAGER] AST parsed OK, fanning out to %zu chunks", chunk_managers.size());

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
        logger::warn("[INDEX_MANAGER] query complete, collected %zu pages", resp.pages.size());
        return resp;
    }

private:
    JoinableThreadPool pool;
    std::atomic<int> next_query_id{0};
    vector<chunk_manager> chunk_managers;
};
