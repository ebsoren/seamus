#pragma once
#include "cstddef"
#include "domain_carousel.h"
#include "lib/consts.h"
#include "lib/logger.h"
#include "network_util.h"
#include "parser/parser.h"
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include "../lib/vector.h"


// Runs in a detached thread, there are CRAWLER_THREADPOOL_SIZE concurrent instances of these
// Monitors an interval [carousel_left, carousel_right] inclusive on the domain carousel
// Makes network call to fetch HTML buffer -> parses -> persists to disk
inline void crawler_worker(DomainCarousel& dc, size_t carousel_left, size_t carousel_right, std::atomic<bool>& running, HtmlParser* parser) {
    while (running) {
        for (size_t carousel_index = carousel_left; running; carousel_index = (carousel_index < carousel_right) ? carousel_index + 1 : carousel_left) {
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
                    if (elapsed >= static_cast<long long>(CRAWLER_BACKOFF_SEC)) {
                        target.emplace(std::move(slot.targets.front()));
                        slot.targets.pop_front();
                        slot.request_last_sent = now;
                    }
                }
            }

            // Sleep if we couldn't acquire a target (empty slot or backoff not elapsed)
            if (!target) {
                std::this_thread::sleep_for(std::chrono::milliseconds(CRAWLER_WORKER_SLEEP_MS));
                continue;
            }
            logger::debug("Worker [%zu-%zu] pulled url: %s", carousel_left, carousel_right, target->url.data());

            // Only crawl HTTPS links
            if (target->url.size() < 8 || memcmp(target->url.data(), "https://", 8) != 0) continue;

            // Extract host and path from URL
            string host = extract_host(target->url);
            const char* slash = static_cast<const char*>(memchr(target->url.data() + 8, '/', target->url.size() - 8));
            const char* path = slash ? slash + 1 : "";

            char body[MAX_HTML_SIZE];
            ssize_t body_len = https_get(host.data(), path, body);
            if (body_len > 0) {
                logger::debug("Worker [%zu-%zu] received %zd bytes from %s", carousel_left, carousel_right, body_len, target->url.data());
                parser->parse_page(body, static_cast<size_t>(body_len), target->seed_distance, target->domain_dist, target->url.data());
            }
        }
    }
}


// Spawns CRAWLER_THREADPOOL_SIZE crawler worker threads, each monitoring an interval of the domain carousel
// Returns the vector of threads so the caller can join them before exiting
inline vector<std::thread> spawn_crawler_workers(DomainCarousel& dc, std::atomic<bool>& running, size_t machine_id) {
    size_t interval_size = CRAWLER_CAROUSEL_SIZE / CRAWLER_THREADPOOL_SIZE;
    size_t curr_domain_left = 0;
    size_t curr_domain_right = interval_size - 1;

    // URL Store (static so it outlives the spawned threads)
    static UrlStore url_store(&dc, my_machine_id());
    logger::info("URL store listener started on port %u with %u threads", URL_STORE_PORT, URL_STORE_NUM_THREADS);

    // Parsers and buffer managers
    static OutboundUrlBuffer outbound(machine_id, &url_store);
    static LocalUrlBuffer url_buffers[NUM_PARSERS];
    static HtmlParser parsers[NUM_PARSERS];
    for (size_t i = 0; i < NUM_PARSERS; i++) {
        url_buffers[i] = LocalUrlBuffer(machine_id, &outbound);
        parsers[i] = HtmlParser(i, &url_buffers[i], &url_store);
    }
    logger::info("Spawned %u parsers and local URL buffers.", NUM_PARSERS);

    vector<std::thread> workers;
    int i = 0;
    while (curr_domain_right < CRAWLER_CAROUSEL_SIZE) {
        workers.push_back(std::thread(crawler_worker, std::ref(dc), curr_domain_left, curr_domain_right, std::ref(running), &parsers[i]));
        curr_domain_left = curr_domain_right + 1;
        curr_domain_right = curr_domain_left + interval_size - 1;
        i++;
    }
    return workers;
}
