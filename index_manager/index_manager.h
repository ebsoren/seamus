#pragma once

#include <cstdint>
#include <thread>

#include "index_chunk/chunk_manager.h"
#include "lib/atomic_vector.h"
#include "lib/consts.h"
#include "lib/query_response.h"
#include "lib/string.h"
#include "lib/utils.h"
#include "lib/vector.h"


class index_manager {
public:
    // Walk every worker's chunk files in order; stop at the first missing
    // chunk for each worker. Mirrors recover_index_chunks, which targets
    // IndexChunk::get_index_chunk_path's naming.
    index_manager() {
        for (uint32_t w = 0; w < NUM_INDEXER_THREADS; ++w) {
            for (uint32_t c = 0;; ++c) {
                string path = string::join("", string(INDEX_OUTPUT_DIR), "/index_chunk_", string(w), "_",
                                           string(c), ".txt");
                if (!file_exists(path)) break;
                chunk_managers.emplace_back(path);
            }
        }
    }

    index_manager(const index_manager &)            = delete;
    index_manager &operator=(const index_manager &) = delete;

    // Fan `words` out to every chunk's leapfrog query and gather the merged
    // DocInfos into a QueryResponse. Query parsing (tokenize/dedup/etc.) is
    // the caller's responsibility — see query/expressions.h. Blocks until
    // every worker has joined, so the lambda captures stay valid.
    // INVARIANT: words must contain unique elements.
    QueryResponse handle_query(const vector<string> &words) {
        atomic_vector<DocInfo> collector;

        vector<std::thread> threads;
        threads.reserve(chunk_managers.size());
        for (chunk_manager &cm : chunk_managers) {
            threads.push_back(std::thread([&cm, &words, &collector]() {
                cm.default_query(words, &collector);
            }));
        }
        for (std::thread &t : threads) {
            if (t.joinable()) t.join();
        }

        QueryResponse resp;
        resp.pages = collector.take();
        return resp;
    }

private:
    vector<chunk_manager> chunk_managers;
};
