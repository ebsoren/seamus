#pragma once

#include <cassert>
#include <cstddef>
#include "vector.h"
#include "string.h"


// Logging (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR, 4=NONE)
constexpr uint8_t LOG_LEVEL = 0;


// Global
constexpr size_t NUM_MACHINES = 1;                                  // todo(hershey): obviously, change when we deploy on more machines
constexpr const char* MACHINES[NUM_MACHINES] = {"127.0.0.1"};       // todo(hershey): replace localhost ip (127.0.0.1) with global ip of machines once we deploy on multiple machines -- store machine ID as an environment variable

inline const char* get_machine_addr(size_t machine_id) {
    // todo(hershey): once we deploy on multiple machines, check an environment variable here (e.g., self_id) and return localhost if machine_id == self_id

    assert(machine_id < NUM_MACHINES);
    return MACHINES[machine_id];
}


// Crawler
constexpr uint16_t CRAWLER_LISTENER_PORT = 8080;
constexpr size_t CRAWLER_LISTENER_THREADS = 16;
constexpr size_t CRAWLER_CAROUSEL_SIZE = 8192;
static constexpr size_t CRAWLER_CAROUSEL_QUEUE_SIZE = 32;
constexpr size_t CRAWLER_THREADPOOL_SIZE = 512;
constexpr size_t CRAWLER_MAX_QUEUE_SIZE = 32;
static_assert(CRAWLER_CAROUSEL_SIZE % CRAWLER_THREADPOOL_SIZE == 0, "[consts.h]: CRAWLER_CAROUSEL_SIZE must be a multiple of CRAWLER_THREADPOOL_SIZE");
static_assert(CRAWLER_CAROUSEL_SIZE >= CRAWLER_THREADPOOL_SIZE, "[consts.h]: CRAWLER_THREADPOOL_SIZE cannot be greater than CRAWLER_CAROUSEL_SIZE");

constexpr size_t CRAWLER_BACKOFF_SEC = 2;           // Time (seconds) to wait between sending GET requests to URLs within the same domain carousel slot
constexpr size_t PERSIST_INTERVAL_SEC = 60;         // Time (seconds) to wait between persists of in-memory priority buckets -> disk priority bucket files
constexpr size_t FEED_INTERVAL_SEC = 1;             // Time (seconds) to wait between feeding in-memory priority buckets -> domain carousel

constexpr size_t PRIORITY_BUCKETS = 8;
inline vector<string> get_frontier_bucket_files() {
    vector<string> files;
    for (size_t i = 0; i < PRIORITY_BUCKETS; ++i) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "bucket_p%zu", i);
        files.push_back(string(buf, static_cast<size_t>(len)));
    }
    return files;
}
