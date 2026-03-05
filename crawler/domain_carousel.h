#pragma once

#include "../lib/deque.h"
#include "../lib/rpc_crawler.h"
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
};
