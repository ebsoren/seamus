#pragma once
#include "cstddef"
#include "domain_carousel.h"
#include "lib/consts.h"
#include "network_util.h"
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>


// Runs in a detached thread, there are CRAWLER_THREADPOOL_SIZE concurrent instances of these
// Monitors an interval [carousel_left, carousel_right] inclusive on the domain carousel
// Makes network call to fetch HTML buffer -> parses -> persists to disk
inline void crawler_worker(DomainCarousel& dc, size_t carousel_left, size_t carousel_right, std::atomic<bool>& running) {
    while (running) {
        for (size_t carousel_index = carousel_left; running; carousel_index = (carousel_index < carousel_right) ? carousel_index + 1 : carousel_left) {
            // Check current slot
            // Fetch HTML buffer and reset `request_last_sent` if enough time has elapsed
            std::optional<CrawlTarget> target;
            {
                std::lock_guard<std::mutex> lock(dc.carousel[carousel_index].domain_queue_lock);
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

            // Sleep if we couldn't acquire a target
            if (!target) {
                std::this_thread::sleep_for(std::chrono::milliseconds(CRAWLER_WORKER_SLEEP_MS));
                continue;
            }

            // TODO(hershey): At this point, we know that the url belongs to this machine, so we can update url store with num url hops and num domain hops from the seed list
            // TODO(Esben/David): Parser needs to send updates to urlstore(s) (potentially on other machines) with anchor text data that it finds

            // Only crawl HTTPS links
            if (target->url.size() < 8 || memcmp(target->url.data(), "https://", 8) != 0) continue;

            // Extract path from URL (everything after the host)
            const char* slash = static_cast<const char*>(memchr(target->url.data() + 8, '/', target->url.size() - 8));
            const char* path = slash ? slash + 1 : "";

            size_t out_len = 0;
            char* body = https_get(target->domain.data(), path, &out_len);
            if (body) {
                // TODO(Esben/David): call inlined parser logic here on the raw body buffer, make sure you free the buffer after the parsed contents are persisted (see below)
                free(body);
            }
        }
    }
}


// Spawns CRAWLER_THREADPOOL_SIZE detached crawler worker threads, each monitoring an interval of the domain carousel
inline void spawn_crawler_workers(DomainCarousel& dc, std::atomic<bool>& running) {
    size_t interval_size = CRAWLER_CAROUSEL_SIZE / CRAWLER_THREADPOOL_SIZE;
    size_t curr_domain_left = 0;
    size_t curr_domain_right = interval_size - 1;

    while (curr_domain_right < CRAWLER_CAROUSEL_SIZE) {
        std::thread(crawler_worker, std::ref(dc), curr_domain_left, curr_domain_right, std::ref(running)).detach();
        curr_domain_left = curr_domain_right + 1;
        curr_domain_right = curr_domain_left + interval_size - 1;
    }
}
