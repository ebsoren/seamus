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
#include "crawler_metrics.h"
#include "domain_carousel.h"
#include "bucket_manager.h"
#include <dirent.h>


class CrawlerInstrumentation {
public:
    CrawlerInstrumentation(size_t num_workers, size_t drain_interval_sec = CRAWLER_INSTRUMENTATION_INTERVAL_SEC)
        : drain_interval_sec(drain_interval_sec), queues(num_workers), locks(num_workers), start_time(std::chrono::steady_clock::now()) {}

    void set_url_store(UrlStore* us) { url_store = us; }
    void set_carousel(DomainCarousel* dc) { carousel = dc; }
    void set_bucket_manager(BucketManager* bm) { bucket_manager = bm; }

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
    std::atomic<uint64_t> urls_from_local{0};
    std::atomic<uint64_t> urls_from_rpc{0};
    uint64_t prev_urls_from_local{0};
    uint64_t prev_urls_from_rpc{0};
    std::atomic<bool> running{true};
    std::mutex shutdown_mutex;
    std::condition_variable shutdown_cv;
    std::thread drain_thread;
    std::chrono::steady_clock::time_point start_time;
    UrlStore* url_store = nullptr;
    DomainCarousel* carousel = nullptr;
    BucketManager* bucket_manager = nullptr;

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
                    case MetricType::LOCAL_URL_ACCUMULATE:
                        urls_from_local.fetch_add(static_cast<uint64_t>(update.num), std::memory_order_relaxed);
                        break;
                    case MetricType::RECEIVED_URL_ACCUMULATE:
                        urls_from_rpc.fetch_add(static_cast<uint64_t>(update.num), std::memory_order_relaxed);
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
            double avg_docs_per_sec = elapsed > 0 ? static_cast<double>(current) / static_cast<double>(elapsed) : 0.0;

            logger::instr("%02ld:%02ld:%02ld | total docs: %llu | docs/sec: %.1f | avg docs/sec: %.1f | avg page len: %.1f bytes | avg priority: %.2f",
                hrs, mins, secs, current, docs_per_sec, avg_docs_per_sec, get_avg_page_length(), get_avg_page_priority());

            uint64_t cur_local = urls_from_local.load(std::memory_order_relaxed);
            uint64_t cur_rpc = urls_from_rpc.load(std::memory_order_relaxed);
            uint64_t interval_local = cur_local - prev_urls_from_local;
            uint64_t interval_rpc = cur_rpc - prev_urls_from_rpc;
            double local_per_sec = drain_interval_sec > 0 ? static_cast<double>(interval_local) / drain_interval_sec : 0;
            double rpc_per_sec = drain_interval_sec > 0 ? static_cast<double>(interval_rpc) / drain_interval_sec : 0;
            prev_urls_from_local = cur_local;
            prev_urls_from_rpc = cur_rpc;
            logger::instr("local urls: %llu (%.1f/sec) | rpc urls: %llu (%.1f/sec)",
                cur_local, local_per_sec, cur_rpc, rpc_per_sec);

            if (url_store) {
                size_t seen = url_store->seen_urls();
                size_t distinct = url_store->distinct_urls();
                size_t interval_seen = seen - prev_urls_seen;
                size_t interval_distinct = distinct - prev_urls_distinct;
                double total_per_sec = drain_interval_sec > 0 ? static_cast<double>(interval_seen) / drain_interval_sec : 0;
                double new_per_sec = drain_interval_sec > 0 ? static_cast<double>(interval_distinct) / drain_interval_sec : 0;
                prev_urls_seen = seen;
                prev_urls_distinct = distinct;
                logger::instr("total urls: %zu | total urls/sec: %.1f | distinct urls: %zu | new urls/sec: %.1f",
                    seen, total_per_sec, distinct, new_per_sec);

                auto ms = url_store->mem_stats();
                double slots_mb = ms.shard_slots_bytes / 1e6;
                double entry_mb = ms.shard_entry_heap / 1e6;
                double anchor_mb = ms.anchor_dict_bytes / 1e6;
                double total_mb = slots_mb + entry_mb + anchor_mb;
                double load = ms.total_slots > 0 ? static_cast<double>(ms.total_entries) / ms.total_slots : 0;
                logger::instr("mem | urlstore total: %.1f MB | shard slots: %.1f MB | entry heap: %.1f MB | anchor dict: %.1f MB | load: %.2f (%zu/%zu)\n",
                    total_mb, slots_mb, entry_mb, anchor_mb, load, ms.total_entries, ms.total_slots);
            }

            // Frontier health: carousel fill, per-priority bucket sizes, backoff queue.
            // Try-locks throughout so drain never blocks production threads. Slots
            // that are locked are counted as non-empty (a worker is servicing them).
            if (carousel) {
                size_t nonempty_slots = 0;
                size_t locked_slots = 0;
                size_t total_targets = 0;
                size_t max_slot_depth = 0;
                for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
                    std::unique_lock<std::mutex> lk(carousel->carousel[i].domain_queue_lock, std::try_to_lock);
                    if (!lk.owns_lock()) { locked_slots++; nonempty_slots++; continue; }
                    size_t sz = carousel->carousel[i].targets.size();
                    if (sz > 0) {
                        nonempty_slots++;
                        total_targets += sz;
                        if (sz > max_slot_depth) max_slot_depth = sz;
                    }
                }
                double fill_pct = 100.0 * static_cast<double>(nonempty_slots) / static_cast<double>(CRAWLER_CAROUSEL_SIZE);
                logger::instr("carousel | slots used: %zu/%zu (%.1f%%) | locked: %zu | targets queued: %zu | max/slot: %zu",
                    nonempty_slots, CRAWLER_CAROUSEL_SIZE, fill_pct,
                    locked_slots, total_targets, max_slot_depth);

                char bbuf[256];
                int off = snprintf(bbuf, sizeof(bbuf), "priority buckets |");
                for (size_t p = 0; p < PRIORITY_BUCKETS && off < static_cast<int>(sizeof(bbuf)); ++p) {
                    std::unique_lock<std::mutex> lk(carousel->buckets[p].bucket_lock, std::try_to_lock);
                    size_t sz = lk.owns_lock() ? carousel->buckets[p].urls.size() : 0;
                    const char* tag = lk.owns_lock() ? "" : "?";
                    off += snprintf(bbuf + off, sizeof(bbuf) - static_cast<size_t>(off), " p%zu=%zu%s", p, sz, tag);
                }
                logger::instr("%s", bbuf);
            }
            if (bucket_manager) {
                size_t backoff_size = 0;
                bool backoff_observed = false;
                {
                    std::unique_lock<std::mutex> lk(bucket_manager->backoff_lock, std::try_to_lock);
                    if (lk.owns_lock()) {
                        backoff_size = bucket_manager->backoff_queue.size();
                        backoff_observed = true;
                    }
                }
                logger::instr("backoff queue: %zu%s",
                    backoff_size, backoff_observed ? "" : " (lock busy)");
            }

            size_t fd_count = 0;
            if (DIR* d = opendir("/proc/self/fd")) {
                while (readdir(d)) fd_count++;
                closedir(d);
                fd_count -= 2; // . and ..
            }
            logger::instr("open fds: %zu\n", fd_count);
        }
    }
};
