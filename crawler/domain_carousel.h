//
// Created by Aiden Mizhen on 2/23/26.
//

#pragma once
#include "lib/consts.h"
#include "lib/deque.h"
#include "lib/string.h"
#include "lib/utils.h"
#include "lib/unordered_map.h"
#include <chrono>
#include <mutex>
#include <thread>



using namespace std::chrono_literals;

struct CrawlTarget {
    string domain;
    string url;
};


class DomainCarousel {

public:
    static constexpr size_t MAX_SIZE = CRAWLER_CAROUSEL_SIZE*CRAWLER_CAROUSEL_QUEUE_SIZE;

private:
    std::atomic<size_t> size = 0;
    static constexpr  std::chrono::steady_clock::duration WAIT_TIME = 2s; // TODO : Switch to env? or make website specific
    static constexpr  std::chrono::steady_clock::duration SLEEP_TIME = 20ms;


    struct alignas(64) DomainQueue {
        std::mutex domain_lock;
        std::chrono::steady_clock::time_point ready_at;
        deque<CrawlTarget> targets{};
    };
    DomainQueue carousel[CRAWLER_CAROUSEL_SIZE]{};


    static size_t hash_domain(const string& domain) {
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

    void test() {
        auto m = unordered_map<int, string>();
    }


public:
    CrawlTarget get_target(uint32_t thread_num = 0) {
        const size_t start_index = CRAWLER_CAROUSEL_SIZE * thread_num / CRAWLER_THREADPOOL_SIZE;
        size_t domain_index = start_index;

        std::chrono::steady_clock::time_point time = std::chrono::steady_clock::now();
        while (true) {
            if (not carousel[domain_index].targets.empty()) {
                std::unique_lock<std::mutex> lock(carousel[domain_index].domain_lock, std::try_to_lock);
                if (lock.owns_lock() and not carousel[domain_index].targets.empty()
                    and time >= carousel[domain_index].ready_at) {

                    carousel[domain_index].ready_at = time + WAIT_TIME;
                    CrawlTarget target = move(carousel[domain_index].targets.front());
                    carousel[domain_index].targets.pop_front();
                    size.fetch_sub(1, std::memory_order_relaxed);

                    return target;
                }
            }
            domain_index = (domain_index + 1) % CRAWLER_CAROUSEL_SIZE;
            if (domain_index == start_index) {
                std::this_thread::sleep_for(SLEEP_TIME);
                time = std::chrono::steady_clock::now();
            }
        }
    }

    bool push_target(CrawlTarget&& target) {
        const size_t domain_index = hash_domain(target.domain) % CRAWLER_CAROUSEL_SIZE;
        if (carousel[domain_index].targets.size() >= CRAWLER_CAROUSEL_QUEUE_SIZE) {
            return false;
        }
        std::lock_guard<std::mutex> lock(carousel[domain_index].domain_lock);
        if (carousel[domain_index].targets.size() >= CRAWLER_CAROUSEL_QUEUE_SIZE) {
            return false;
        }
        carousel[domain_index].targets.push_back(move(target));
        size.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // TODO(Aiden) :
    //  need a way to deal with the first 100k webpages being wikipedia.com
    //  Likely issues:
    //      * Break the re-fill logic, will think carousel is full but its all in wikipedia bucket
    //  Solutions
    //      * First set bucket limits (required) + mad push_target return bool, success/fail
    //      * Keep track of min bucket fill level, not total urls
    //      * These don't fix the 100k wikipedia issue. Has 2 possible solutions:
    //          - Keep an overflow queue (basically a second frontier, only pull when a "max fill"
    //              variable is below a threshold
    //          - Much simpler: Just push the overflowing url to the back of the frontier
    //      * Or we say this isn't a problem and insist we want to do wikipedia first
};
