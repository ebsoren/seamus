#pragma once

#include "../lib/deque.h"
#include "../lib/rpc_crawler.h"
#include "../lib/consts.h"
#include "../lib/consts.h"
#include <mutex>
#include <chrono>
#include <cstdint>


class DomainCarousel {
public:

    // In-memory priority buckets of URLs, maintain locks per priority bucket
    struct alignas(64) PriorityBucket {
        std::mutex bucket_lock;
        deque<CrawlTarget> urls{};
    };
    PriorityBucket buckets[PRIORITY_BUCKETS]{};


    // Queue and carousel state
    struct alignas(64) DomainQueue {
        std::mutex domain_queue_lock;
        deque<CrawlTarget> targets{};
        std::chrono::steady_clock::time_point request_last_sent{};
    };
    DomainQueue carousel[CRAWLER_CAROUSEL_SIZE]{};


    // Domain hash function
    static size_t hash_domain(const string& domain) {
        // These constants are specific to 64-bit FNV-1
        size_t hash = 0xcbf29ce484222325ULL;
        size_t fnv_prime = 0x100000001b3ULL;

        for (size_t i = 0; i < domain.size(); ++i) {
            // XOR the bottom byte
            hash ^= static_cast<unsigned char>(domain[i]);
            // Multiply by the prime
            hash *= fnv_prime;
        }

        return hash;
    }


    // Push crawl target to appropriate slot in carousel
    // Return bool result representing success, lock guard on the respective queue and ensure pushing the target will not exceed size
    bool push_target(CrawlTarget&& target) {
        const size_t domain_index = hash_domain(target.domain) % CRAWLER_CAROUSEL_SIZE;
        std::lock_guard<std::mutex> lock(carousel[domain_index].domain_queue_lock);    
        if (carousel[domain_index].targets.size() >= CRAWLER_MAX_QUEUE_SIZE) {
            return false;
        }

        carousel[domain_index].targets.push_back(std::move(target));
        return true;
    }


    // Util function to feed the carousel with the highest priority bucket that is populated
    // Returns the priority level of the bucket that was emptied into the carousel, -1 on error/all buckets being empty
    int16_t feed_carousel() {
        for (size_t plevel = 0; plevel < PRIORITY_BUCKETS; ++plevel) {
            {
                std::lock_guard<std::mutex> lock(buckets[plevel].bucket_lock);
                if (buckets[plevel].urls.size() > 0) {
                    while (buckets[plevel].urls.size() > 0) {
                        push_target(std::move(buckets[plevel].urls.front()));
                        buckets[plevel].urls.pop_front();
                    }
                    return static_cast<int16_t>(plevel);
                }
            }
        }

        return -1;
    }
};
