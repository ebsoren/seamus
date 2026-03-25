#pragma once

#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/deque.h"
#include "../lib/rpc_crawler.h"
#include "lib/consts.h"
#include "domain_carousel.h"
#include <cassert>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <fstream>


class BucketManager {
public:

    std::mutex backoff_lock;
    deque<BackoffEntry> backoff_queue;


    BucketManager(vector<string> bucket_files_in, DomainCarousel* dc_in)
        : bucket_files(static_cast<vector<string>&&>(bucket_files_in)), dc(dc_in) {
        assert(bucket_files.size() == PRIORITY_BUCKETS);
        for (const auto& path : bucket_files) {
            std::ifstream check(path.data());
            if (!check.good()) {
                std::ofstream create(path.data());
            }
        }
    }


    static size_t get_priority_bucket(const string& url) {
        // TODO(hershey): delete this after Erik implements the same interface in Frontier, Frontier doesn't yet build and doesn't follow lib structure
        // 0 is the index of the highest priority bucket, PRIORITY_BUCKETS - 1 is the index of the lowest priority bucket
        // PRIORITY_BUCKETS defined in ~/lib/consts.h
        return 0;
    }


    // Sequentially spawns detached bucket manager routines (below)
    // Sleep 500ms between starting each routine:
    //      1) Load disk buckets into in-memory buckets
    //      2) Feed carousel
    //      3) Persist carousel
    void start() {
        feed_thread = std::thread(&BucketManager::feed_carousel_worker, this);
    }


    // Destructor
    ~BucketManager() {
        running.store(false, std::memory_order_relaxed);
        if (feed_thread.joinable()) feed_thread.join();
    }


    // Runs as a detached, long-lived routine to feed the carousel from the buckets (using feed_carousel() as a util function)
    // This 1) tries to move crawl targets from priority buckets into the carousel and 2) tries to move crawl targets from the backoff queue into the carousel
    void feed_carousel_worker() {
        while (running) {
            // Move crawl targets from priority buckets into the carousel
            dc->feed_carousel_from_highest_priority_bucket(backoff_lock, backoff_queue);

            // Move crawl targets from the backoff queue into the carousel, breaks once we encounter an item that has not been on the backoff queue for long enough (FIFO order)
            {
                auto now = std::chrono::steady_clock::now();
                std::lock_guard<std::mutex> lock(backoff_lock);
                while (!backoff_queue.empty()) {
                    auto& front = backoff_queue.front();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - front.rejected_at).count();
                    if (elapsed < static_cast<long long>(CRAWLER_BACKOFF_SEC)) break;

                    CrawlTarget target = std::move(front.target);
                    backoff_queue.pop_front();
                    if (!dc->push_target(std::move(target))) {
                        backoff_queue.push_back(BackoffEntry{std::move(target), now});
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(CRAWLER_FEED_INTERVAL_SEC));
        }
    }


    // Load disk buckets into in-memory buckets
    // Should be called ad-hoc by feed_carousel_worker() whenever more crawl targets are needed in memory
    void load_disk_buckets() {
        for (size_t i = 0; i < PRIORITY_BUCKETS; ++i) {
            std::ifstream file(bucket_files[i].data(), std::ios::binary | std::ios::ate);
            if (!file.good()) continue;

            auto file_size = file.tellg();
            if (file_size <= 0) continue;

            file.seekg(0, std::ios::beg);
            size_t size = static_cast<size_t>(file_size);
            char* buf = new char[size];
            file.read(buf, static_cast<std::streamsize>(size));

            const char* cursor = buf;
            size_t remaining = size;

            std::lock_guard<std::mutex> lock(dc->buckets[i].bucket_lock);
            while (remaining > 0) {
                CrawlTarget ct{string(""), string(""), 0, 0};
                const char* next = deserialize_crawl_target(cursor, remaining, ct);
                if (!next) break;
                remaining -= static_cast<size_t>(next - cursor);
                cursor = next;
                dc->buckets[i].enqueue(std::move(ct));
            }

            delete[] buf;
        }
    }


    // todo(hershey): write helper to persist in-memory buckets into disk buckets
    // This should run in a detached thread on an interval (PERSIST_INTERVAL_SEC in consts.h)


private:
  
    // bucket_files[priority] = file to serialized queue of urls
    // index 0 is highest priority, index PRIORITY_BUCKETS - 1 is the lowest
    vector<string> bucket_files;

    // Domain carousel that we are managing
    DomainCarousel* dc;

    // Signal for detached threads to exit (and thus join in the destructor)
    std::atomic<bool> running{true};

    // Nameable threads for joining
    std::thread feed_thread;
};
