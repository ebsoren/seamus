#pragma once

#include "../lib/vector.h"
#include "../lib/rpc_crawler.h"
#include "../lib/consts.h"
#include "../lib/logger.h"
#include "../lib/string.h"
#include <mutex>


class CrawlerOutbound {
public:

    CrawlerOutbound() {
        for (size_t i = 0; i < NUM_MACHINES; ++i) {
            buffers[i] = vector<CrawlTarget>();
        }
    }


    // Add a crawl target to the buffer for the given machine
    // If the buffer hits CRAWLER_OUTBOUND_BATCH_SIZE, flush it to that machine's crawler listener
    void add(size_t machine_id, CrawlTarget&& target) {
        std::lock_guard<std::mutex> lock(locks[machine_id]);
        buffers[machine_id].push_back(static_cast<CrawlTarget&&>(target));

        if (buffers[machine_id].size() >= CRAWLER_OUTBOUND_BATCH_SIZE) {
            flush_locked(machine_id);
        }
    }


private:

    vector<CrawlTarget> buffers[NUM_MACHINES];
    std::mutex locks[NUM_MACHINES];

    // TODO: Refactor --
    // Flush_locked should add a pointer switch before this
    // Create a new buffer and swap it into the array, then unlock
    // 
    // Can unlock once you switch the pointers

    // Sends the buffered targets to the machine's crawler listener and clears the buffer
    // Caller must hold locks[machine_id]
    void flush_locked(size_t machine_id) {
        BatchCrawlTargetRequest batch;
        batch.targets = static_cast<vector<CrawlTarget>&&>(buffers[machine_id]);
        buffers[machine_id] = vector<CrawlTarget>();

        const char* addr = get_machine_addr(machine_id);
        string host(addr, strlen(addr));
        if (!send_batch_crawl_target_request(host, CRAWLER_LISTENER_PORT, batch)) {
            logger::warn("Failed to send outbound batch to machine %zu (%s)", machine_id, addr);
        }
    }
};
