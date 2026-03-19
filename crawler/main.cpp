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
    logger::info("Crawler started...");

    // TODO: Assign correct machine ID value
    size_t machine_id = 0;

    // Initialize crawler components/modules
    // Domain carousel
    DomainCarousel dc;
    logger::info("Domain carousel initialized (%zu hash slots, max %zu per queue)", CRAWLER_CAROUSEL_SIZE, CRAWLER_MAX_QUEUE_SIZE);

    // URL Store
    UrlStore url_store(&dc);
    logger::info("URL store listener started on port %u with %u threads", URL_STORE_PORT, URL_STORE_NUM_THREADS);

    // Parsers and buffer managers
    OutboundUrlBuffer outbound(machine_id, &url_store);
    LocalUrlBuffer url_buffers[NUM_PARSERS];
    HtmlParser parsers[NUM_PARSERS];
    for (size_t i = 0; i < NUM_PARSERS; i++) {
        url_buffers[i] = LocalUrlBuffer(machine_id, &outbound);
        
        parsers[i] = HtmlParser(i, &url_buffers[i], &url_store);
    }
    logger::info("Spawned %u parsers and local URL buffers.", NUM_PARSERS);

    // Bucket manager
    vector<string> bucket_files = get_frontier_bucket_files();
    logger::info("Initialized %zu bucket files:", bucket_files.size());
    for (size_t i = 0; i < bucket_files.size(); ++i) {
        logger::info("  bucket[%zu] = %.*s", i, static_cast<int>(bucket_files[i].size()), bucket_files[i].data());
    }
    BucketManager bm(static_cast<vector<string>&&>(bucket_files), &dc);
    bm.start();

    // Crawler listener
    CrawlerListener cl(&bm, &dc);
    cl.start();
    logger::info("Crawler listener started on port %u with %zu threads", CRAWLER_LISTENER_PORT, CRAWLER_LISTENER_THREADS);

    // Crawler workers (multiplexing domain carousel)
    std::atomic<bool> workers_running{true};
    spawn_crawler_workers(dc, workers_running);
    logger::info("Spawned %zu crawler workers", CRAWLER_THREADPOOL_SIZE);
}
