// Crawler driver

#include "bucket_manager.h"
#include "crawler_listener.h"
#include "crawler_worker.h"
#include "domain_carousel.h"
#include "../lib/logger.h"

int main() {
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
    BucketManager bm(static_cast<vector<string>&&>(bucket_files));

    // Crawler listener
    CrawlerListener cl(&bm, &dc);
    cl.start();
    logger::info("Crawler listener started on port %u with %zu threads", CRAWLER_LISTENER_PORT, CRAWLER_LISTENER_THREADS);
}
