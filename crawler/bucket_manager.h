#pragma once

#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/deque.h"
#include "../lib/rpc_crawler.h"
#include "lib/consts.h"
#include "domain_carousel.h"
#include "lib/utils.h"
#include <cassert>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <fstream>
#include <condition_variable>
#include <memory>


class BucketManager {
public:

    static inline bool load_seed_list = true;

    std::mutex backoff_lock;
    deque<BackoffEntry> backoff_queue;


    BucketManager(vector<string> bucket_files_in, DomainCarousel* dc_in)
        : bucket_files(static_cast<vector<string>&&>(bucket_files_in)), dc(dc_in) {
        assert(bucket_files.size() == PRIORITY_BUCKETS);

        // Create bucket files if they don't exist, otherwise don't overwrite existing bucket files
        for (const auto& path : bucket_files) {
            std::ifstream check(path.data());
            if (!check.good()) {
                std::ofstream create(path.data());
            }
        }

        // Load seed list into priority bucket 0 if flag is set (true in non-testing environments) 
        if (load_seed_list) {
            std::lock_guard<std::mutex> lock(dc->buckets[0].bucket_lock);
            for (size_t i = 0; i < SEED_LIST_SIZE; ++i) {
                dc->buckets[0].enqueue(CrawlTarget{extract_domain(string(SEED_LIST[i])), string(SEED_LIST[i]), 0, 0});
            }
        }
    }

    // Sequentially spawns detached bucket manager routines (below)
    // Sleep 500ms between starting each routine:
    //      1) Load disk buckets into in-memory buckets
    //      2) Feed carousel
    //      3) Persist carousel
    void start() {
        feed_thread = std::thread(&BucketManager::feed_carousel_worker, this);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        bucket_persist_thread = std::thread(&BucketManager::persist_buckets_worker, this);
    }


    // Destructor
    ~BucketManager() {
        running.store(false, std::memory_order_relaxed);
        shutdown_cv.notify_all();
        if (feed_thread.joinable()) feed_thread.join();
        if (bucket_persist_thread.joinable()) bucket_persist_thread.join();
        persist_buckets();
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


    // Load disk buckets into in-memory buckets, should be called on startup
    void load_disk_buckets() {
        for (size_t i = 0; i < PRIORITY_BUCKETS; ++i) {
            std::ifstream file(bucket_files[i].data(), std::ios::binary | std::ios::ate);
            if (!file.good()) continue;

            auto file_size = file.tellg();
            if (file_size <= 0) continue;

            file.seekg(0, std::ios::beg);
            size_t size = static_cast<size_t>(file_size);
            auto buf = std::make_unique<char[]>(size);
            file.read(buf.get(), static_cast<std::streamsize>(size));

            const char* cursor = buf.get();
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
        }
    }


    // Helper routine for persist_buckets_worker()
    void persist_buckets() {
        for (size_t i = 0; i < PRIORITY_BUCKETS; ++i) {
            std::unique_ptr<char[]> buf;
            size_t total_size = 0;

            {
                std::lock_guard<std::mutex> lock(dc->buckets[i].bucket_lock);
                size_t count = dc->buckets[i].urls.size();

                if (count == 0) {
                    std::ofstream file(bucket_files[i].data(), std::ios::binary | std::ios::trunc);
                    continue;
                }

                for (size_t j = 0; j < count; ++j) {
                    total_size += crawl_target_serialized_size(dc->buckets[i].urls[j]);
                }

                buf = std::make_unique<char[]>(total_size);
                char* cursor = buf.get();

                for (size_t j = 0; j < count; ++j) {
                    cursor = serialize_crawl_target(cursor, dc->buckets[i].urls[j]);
                }
            }

            std::ofstream file(bucket_files[i].data(), std::ios::binary | std::ios::trunc);
            file.write(buf.get(), static_cast<std::streamsize>(total_size));
        }
    }


    // Detached thread, persists frontier buckets on an interval
    void persist_buckets_worker() {
        while (running) {
            std::unique_lock<std::mutex> lock(shutdown_mutex);
            shutdown_cv.wait_for(lock, std::chrono::seconds(CRAWLER_PERSIST_INTERVAL_SEC));
            if (!running) break;
            persist_buckets();
        }
    }


private:
  
    // bucket_files[priority] = file to serialized queue of urls
    // index 0 is highest priority, index PRIORITY_BUCKETS - 1 is the lowest
    vector<string> bucket_files;

    // Domain carousel that we are managing
    DomainCarousel* dc;

    // Signal for detached threads to exit (and thus join in the destructor)
    std::atomic<bool> running{true};
    std::mutex shutdown_mutex;
    std::condition_variable shutdown_cv;

    // Nameable threads for joining
    std::thread feed_thread;
    std::thread bucket_persist_thread;
};
