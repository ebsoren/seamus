#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>

#include <dirent.h>

#include "index/Index.h"
#include "index_chunk/chunk_manager.h"
#include "isr.h"
#include "lib/algorithm.h"
#include "lib/consts.h"
#include "lib/logger.h"
#include "lib/string.h"
#include "lib/utils.h"
#include "lib/vector.h"


class index_manager {
public:
    index_manager() {
        // Number of chunks
        size_t entries = 0;
        struct dirent *entry = nullptr;
        DIR *d = nullptr;
        d = opendir(INDEX_OUTPUT_DIR);
        while ((entry = readdir(d))) {
            // Avoid non-targets
            if (entry->d_name[0] == '.'
                && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')
                    || entry->d_type != DT_REG))
                continue;
            entries++;
            string filepath(INDEX_OUTPUT_DIR + "/" + entry->d_name);

            if (!filepath) {
                // Add log
                continue;
            }

            chunk_managers.emplace_back(chunk_manager(filepath));
        }
        closedir(d);
    }

    QueryResponse handle_query(const string &query) {
        vector<thread> threads(chunk_managers.size());
        for (const auto &worker : chunk_managers) {
            threads.emplace_back(worker.default_query(), words);
        }
        for (auto &t : threads) {
            if (t.joinable()) t.join();
        }
    }

private:
    vector<chunk_manager> chunk_managers;
}
