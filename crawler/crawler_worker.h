#pragma once

#include "crawler/bucket_manager.h"
#include "cstddef"
#include "domain_carousel.h"
#include "lib/Frontier.h"
#include "lib/consts.h"
#include "lib/logger.h"
#include "network_util.h"
#include "parser/parser.h"
#include "parser/RobotsManager.h"
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include "../lib/vector.h"
#include "url_store/url_store.h"
#include "crawler_instrumentation.h"


// Runs in a detached thread, there are CRAWLER_THREADPOOL_SIZE concurrent instances of these
// Monitors an interval [carousel_left, carousel_right] inclusive on the domain carousel
// Makes network call to fetch HTML buffer -> parses -> persists to disk
inline void crawler_worker(DomainCarousel& dc, size_t carousel_left, size_t carousel_right, std::atomic<bool>& running, HtmlParser* parser, RobotsManager* rm, UrlStore* url_store, size_t worker_id, CrawlerInstrumentation* instrumentation) {
    size_t batch_count = 0;
    double batch_page_length = 0;
    double batch_page_priority = 0;

    while (running) {
        bool found_any = false;
        auto min_ready_time = std::chrono::steady_clock::time_point::max();
        for (size_t carousel_index = carousel_left; running && carousel_index <= carousel_right; carousel_index++) {
            // Try lock on the carousel slot - if contended, skip to next slot
            std::optional<CrawlTarget> target;
            {
                std::unique_lock<std::mutex> lock(dc.carousel[carousel_index].domain_queue_lock, std::try_to_lock);
                if (!lock.owns_lock()) {
                    continue;
                }
                auto& slot = dc.carousel[carousel_index];
                if (!slot.targets.empty()) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - slot.request_last_sent).count();
                    if (elapsed >= static_cast<long long>(CRAWLER_BACKOFF_SEC)) { // TODO(charlie): revisit if we want to consider crawl delays returned from robots.txt parsing
                        target.emplace(std::move(slot.targets.front()));
                        slot.targets.pop_front();
                        slot.request_last_sent = now;
                    } else {
                        // Track when the earliest backed-off slot becomes ready
                        auto ready_at = slot.request_last_sent + std::chrono::seconds(CRAWLER_BACKOFF_SEC);
                        if (ready_at < min_ready_time) min_ready_time = ready_at;
                    }
                }
            }

            if (!target) continue;
            found_any = true;
            logger::debug("Worker [%zu-%zu] pulled url: %s", carousel_left, carousel_right, target->url.data());

            // Only crawl HTTPS links
            if (target->url.size() < 8 || memcmp(target->url.data(), "https://", 8) != 0) continue;

            // Skip URLs we've already crawled
            if (url_store->hasCrawled(target->url)) {
                logger::debug("Worker [%zu-%zu] skipping already crawled url: %s", carousel_left, carousel_right, target->url.data());
                continue;
            }

            // Extract host and path from URL
            const string host = extract_host(target->url);
            CrawlStatus status = rm->checkStatus(host, target->url);
            if (status == CrawlStatus::DISALLOWED) {
                logger::debug("Worker [%zu-%zu] skipping disallowed url: %s", carousel_left, carousel_right, target->url.data());
                continue;
            } else {
                logger::debug("Worker [%zu-%zu] allowed url: %s", carousel_left, carousel_right, target->url.data());
            }
            const char* slash = static_cast<const char*>(memchr(target->url.data() + 8, '/', target->url.size() - 8));
            const char* path = slash ? slash + 1 : "";

            auto body = std::make_unique<char[]>(MAX_HTML_SIZE); // TODO : Switched to heap allocation due to stack overflow, switch back maybe?
            ssize_t body_len = https_get(host.data(), path, body.get());
            url_store->markCrawled(target->url);
            if (body_len > 0) {
                logger::debug("Worker [%zu-%zu] received %zd bytes from %s", carousel_left, carousel_right, body_len, target->url.data());
                parser->parse_page(body.get(), static_cast<size_t>(body_len), target->seed_distance, target->domain_dist, target->url.data());

                // Instrumentation calls after parsing
                batch_count++;
                batch_page_length += static_cast<double>(body_len);
                batch_page_priority += 0;                                           // todo(hershey): replace this placeholder with actual url priority function 
                if (batch_count >= CRAWLER_INSTRUMENTATION_BATCH_SIZE) {
                    instrumentation->submit(worker_id, {MetricType::DOCUMENTS_CRAWLED_ACCUMULATE, static_cast<double>(batch_count), 0});
                    instrumentation->submit(worker_id, {MetricType::PAGE_LENGTH_AVERAGE, batch_page_length, static_cast<int>(batch_count)});
                    instrumentation->submit(worker_id, {MetricType::PAGE_PRIORITY_AVERAGE, batch_page_priority, static_cast<int>(batch_count)});
                    batch_count = 0;
                    batch_page_length = 0;
                    batch_page_priority = 0;
                }
            }
        }
        // Only sleep after a full scan found nothing to do
        if (!found_any) {
            if (min_ready_time != std::chrono::steady_clock::time_point::max()) {
                // Sleep until the earliest backed-off slot is ready
                auto wait = min_ready_time - std::chrono::steady_clock::now();
                if (wait > std::chrono::milliseconds(0))
                    std::this_thread::sleep_for(wait);
            } else {
                // All slots empty, no pending backoffs — use default sleep
                std::this_thread::sleep_for(std::chrono::milliseconds(CRAWLER_WORKER_SLEEP_MS));
            }
        }
    }
}


// Spawns CRAWLER_THREADPOOL_SIZE crawler worker threads, each monitoring an interval of the domain carousel
// Returns the vector of threads so the caller can join them before exiting
inline vector<std::thread> spawn_crawler_workers(DomainCarousel& dc, UrlStore& url_store, std::atomic<bool>& running, size_t machine_id, CrawlerInstrumentation& instrumentation) {
    size_t interval_size = CRAWLER_CAROUSEL_SIZE / CRAWLER_THREADPOOL_SIZE;
    size_t curr_domain_left = 0;
    size_t curr_domain_right = interval_size - 1;

    

    // Parsers and buffer managers
    static OutboundUrlBuffer outbound(machine_id, &url_store);
    static LocalUrlBuffer url_buffers[NUM_PARSERS];
    static HtmlParser parsers[NUM_PARSERS];
    static RobotsManager robot_managers[NUM_PARSERS];

    for (size_t i = 0; i < NUM_PARSERS; i++) {
        url_buffers[i] = LocalUrlBuffer(machine_id, &outbound);
        parsers[i] = HtmlParser(i, &url_buffers[i], &url_store);
    }
    logger::info("Spawned %u parsers and local URL buffers.", NUM_PARSERS);

    instrumentation.set_url_store(&url_store);

    vector<std::thread> workers;
    int i = 0;
    while (curr_domain_right < CRAWLER_CAROUSEL_SIZE) {
        workers.push_back(std::thread(crawler_worker, std::ref(dc), curr_domain_left, curr_domain_right, std::ref(running), &parsers[i], &robot_managers[i], &url_store, static_cast<size_t>(i), &instrumentation));
        curr_domain_left = curr_domain_right + 1;
        curr_domain_right = curr_domain_left + interval_size - 1;
        i++;
    }
    return workers;
}
