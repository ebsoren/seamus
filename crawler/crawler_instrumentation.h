#pragma once

#include "../lib/vector.h"
#include "../lib/deque.h"
#include "lib/consts.h"
#include "lib/logger.h"
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

#include "../url_store/url_store.h"


enum class MetricType : uint8_t {
    DOCUMENTS_CRAWLED_ACCUMULATE,           // Accumulator for total # of documents crawled for the entirety of the crawler runtime
    PAGE_LENGTH_AVERAGE,                    // Running average page length
    PAGE_PRIORITY_AVERAGE,                  // Running average of page priority

    // TODO(hershey): implement if needed
    PAGE_LENGTH_DISTRIBUTION,               // Distribution buckets/intervals for page length
    PAGE_PRIORITY_DISTRIBUTION,             // Distribution buckets/intervals for page priority
};

struct MetricUpdate {
    MetricType type;
    double num;                 // numerator quantity
    int den;                    // denominator quantity (for averaging/batching)
};


class CrawlerInstrumentation {
public:
    CrawlerInstrumentation(size_t num_workers, size_t drain_interval_sec = CRAWLER_INSTRUMENTATION_INTERVAL_SEC)
        : drain_interval_sec(drain_interval_sec), queues(num_workers), locks(num_workers), start_time(std::chrono::steady_clock::now()) {}

    void set_url_store(UrlStore* us) { url_store = us; }

    ~CrawlerInstrumentation() {
        running.store(false, std::memory_order_relaxed);
        shutdown_cv.notify_all();
        if (drain_thread.joinable()) drain_thread.join();
    }

    void start() {
        drain_thread = std::thread(&CrawlerInstrumentation::drain_worker, this);
    }

    void submit(size_t worker_id, MetricUpdate update) {
        std::lock_guard<std::mutex> lock(locks[worker_id]);
        queues[worker_id].push_back(update);
    }

    uint64_t get_documents_crawled() { return documents_crawled.load(std::memory_order_relaxed); }
    double get_avg_page_length() {
        uint64_t count = page_length_count.load(std::memory_order_relaxed);
        return count > 0 ? total_page_length.load(std::memory_order_relaxed) / count : 0;
    }
    double get_avg_page_priority() {
        uint64_t count = page_priority_count.load(std::memory_order_relaxed);
        return count > 0 ? total_page_priority.load(std::memory_order_relaxed) / count : 0;
    }

private:
    size_t drain_interval_sec;
    vector<deque<MetricUpdate>> queues;
    vector<std::mutex> locks;

    std::atomic<uint64_t> documents_crawled{0};
    uint64_t prev_documents_crawled{0};
    size_t prev_urls_seen{0};
    size_t prev_urls_distinct{0};
    std::atomic<double> total_page_length{0};
    std::atomic<uint64_t> page_length_count{0};
    std::atomic<double> total_page_priority{0};
    std::atomic<uint64_t> page_priority_count{0};
    std::atomic<bool> running{true};
    std::mutex shutdown_mutex;
    std::condition_variable shutdown_cv;
    std::thread drain_thread;
    std::chrono::steady_clock::time_point start_time;
    UrlStore* url_store = nullptr;

    void process_metric_updates() {
        for (size_t i = 0; i < queues.size(); i++) {
            // Pointer swap on deques
            deque<MetricUpdate> local;
            {
                std::lock_guard<std::mutex> lock(locks[i]);
                local = static_cast<deque<MetricUpdate>&&>(queues[i]);
                queues[i] = deque<MetricUpdate>();
            }

            // Switch handler per update type
            while (!local.empty()) {
                MetricUpdate update = local.front();
                local.pop_front();

                switch (update.type) {
                    case MetricType::DOCUMENTS_CRAWLED_ACCUMULATE:
                        documents_crawled.fetch_add(static_cast<uint64_t>(update.num), std::memory_order_relaxed);
                        break;
                    case MetricType::PAGE_LENGTH_AVERAGE:
                        total_page_length.store(total_page_length.load(std::memory_order_relaxed) + update.num, std::memory_order_relaxed);
                        page_length_count.fetch_add(update.den, std::memory_order_relaxed);
                        break;
                    case MetricType::PAGE_PRIORITY_AVERAGE:
                        total_page_priority.store(total_page_priority.load(std::memory_order_relaxed) + update.num, std::memory_order_relaxed);
                        page_priority_count.fetch_add(update.den, std::memory_order_relaxed);
                        break;
                    default:
                        break;
                }
            }
        }
    }

    void drain_worker() {
        while (running) {
            std::unique_lock<std::mutex> lock(shutdown_mutex);
            shutdown_cv.wait_for(lock, std::chrono::seconds(drain_interval_sec));
            if (!running) break;
            process_metric_updates();

            uint64_t current = get_documents_crawled();
            uint64_t interval_docs = current - prev_documents_crawled;
            double docs_per_sec = drain_interval_sec > 0 ? static_cast<double>(interval_docs) / drain_interval_sec : 0;
            prev_documents_crawled = current;

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
            long hrs = elapsed / 3600;
            long mins = (elapsed % 3600) / 60;
            long secs = elapsed % 60;

            logger::info("Instrumentation | %02ld:%02ld:%02ld | total docs: %llu | docs/sec: %.1f | avg page len: %.1f bytes | avg priority: %.2f",
                hrs, mins, secs, current, docs_per_sec, get_avg_page_length(), get_avg_page_priority());

            if (url_store) {
                size_t seen = url_store->seen_urls();
                size_t distinct = url_store->distinct_urls();
                size_t interval_seen = seen - prev_urls_seen;
                size_t interval_distinct = distinct - prev_urls_distinct;
                double total_per_sec = drain_interval_sec > 0 ? static_cast<double>(interval_seen) / drain_interval_sec : 0;
                double new_per_sec = drain_interval_sec > 0 ? static_cast<double>(interval_distinct) / drain_interval_sec : 0;
                prev_urls_seen = seen;
                prev_urls_distinct = distinct;
                logger::info("Instrumentation | total urls: %zu | total urls/sec: %.1f | distinct urls: %zu | new urls/sec: %.1f",
                    seen, total_per_sec, distinct, new_per_sec);
            }
        }
    }
};
