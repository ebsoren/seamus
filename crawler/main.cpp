// Crawler driver

#include "bucket_manager.h"
#include "crawler_listener.h"
#include "crawler_worker.h"
#include "domain_carousel.h"
#include "../lib/logger.h"
#include "../lib/vector.h"
#include "../lib/consts.h"
#include "../parser/parser.h"
#include "../parser/url_buffers.h"
#include "../url_store/url_store.h"
#include "../parser/RobotsManager.h"
#include <csignal>


inline vector<string> get_frontier_bucket_files() {
    vector<string> files;
    for (size_t i = 0; i < PRIORITY_BUCKETS; ++i) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "bucket_p%zu", i);
        files.push_back(string(buf, static_cast<size_t>(len)));
    }
    return files;
}


int main() {
    signal(SIGPIPE, SIG_IGN);
    logger::info("Crawler started...");

    // Initialize crawler components/modules
    // Domain carousel
    DomainCarousel dc;
    logger::info("Domain carousel initialized (%zu hash slots, max %zu per queue)", CRAWLER_CAROUSEL_SIZE, CRAWLER_MAX_QUEUE_SIZE);

    // Bucket manager
    vector<string> bucket_files = get_frontier_bucket_files();
    logger::info("Initialized %zu bucket files:", bucket_files.size());
    for (size_t i = 0; i < bucket_files.size(); ++i) {
        logger::info("  bucket[%zu] = %.*s", i, static_cast<int>(bucket_files[i].size()), bucket_files[i].data());
    }
    BucketManager bm(static_cast<vector<string>&&>(bucket_files), &dc);
    bm.load_disk_buckets();
    logger::info("Loaded disk buckets into memory");
    bm.start();

    // Crawler listener
    CrawlerListener cl(&bm, &dc);
    cl.start();
    logger::info("Crawler listener started on port %u with %zu threads", CRAWLER_LISTENER_PORT, CRAWLER_LISTENER_THREADS);

    // Crawler workers (multiplexing domain carousel)
    std::atomic<bool> workers_running{true};
    auto workers = spawn_crawler_workers(dc, workers_running, my_machine_id());
    logger::info("Spawned %zu crawler workers", CRAWLER_THREADPOOL_SIZE);

    // Join all worker threads so stack objects stay alive
    for (size_t i = 0; i < workers.size(); ++i) {
        if (workers[i].joinable()) workers[i].join();
    }
}
