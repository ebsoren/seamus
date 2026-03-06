#pragma once

#include "bucket_manager.h"
#include "domain_carousel.h"
#include "../lib/rpc_listener.h"
#include "../lib/rpc_crawler.h"
#include "../lib/consts.h"
#include <thread>


class CrawlerListener {
public:

    CrawlerListener(BucketManager* bm, DomainCarousel* dc)
        : bucket_manager(bm), domain_carousel(dc),
          listener(CRAWLER_LISTENER_PORT, CRAWLER_LISTENER_THREADS) {}


    // Start the listener loop in a detached thread.
    // Incoming BatchCrawlTargetRequests are deserialized and each target is pushed into the appropriate priority bucket.
    void start() {
        std::thread t([this]() {
            listener.listener_loop([this](int fd) {
                auto batch = recv_batch_crawl_target_request(fd);
                close(fd);
                if (!batch) return;

                for (size_t i = 0; i < batch->targets.size(); i++) {
                    size_t bucket_idx = BucketManager::get_priority_bucket(batch->targets[i].url);
                    auto& bucket = domain_carousel->buckets[bucket_idx];
                    std::lock_guard<std::mutex> lock(bucket.bucket_lock);
                    bucket.urls.push_back(static_cast<CrawlTarget&&>(batch->targets[i]));
                }
            });
        });
        t.detach();
    }


    void stop() {
        listener.stop();
    }

private:

    BucketManager* bucket_manager;
    DomainCarousel* domain_carousel;
    RPCListener listener;
};
