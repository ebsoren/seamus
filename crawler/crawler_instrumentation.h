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
        : drain_interval_sec(drain_interval_sec), queues(num_workers), locks(num_workers) {}

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
    std::atomic<double> total_page_length{0};
    std::atomic<uint64_t> page_length_count{0};
    std::atomic<double> total_page_priority{0};
    std::atomic<uint64_t> page_priority_count{0};
    std::atomic<bool> running{true};
    std::mutex shutdown_mutex;
    std::condition_variable shutdown_cv;
    std::thread drain_thread;

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

            logger::info("Instrumentation | total docs: %llu | docs/sec: %.1f | avg page len: %.1f bytes | avg priority: %.2f",
                current, docs_per_sec, get_avg_page_length(), get_avg_page_priority());
        }
    }
};
