#include <iostream>
#include <cassert>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include "lib/string.h"
#include "crawler/domain_carousel.h"


// Include your custom string header and domain carousel here
// #include "lib/string.h"
// #include "domain_carousel.h"

void test_basic_push_pop() {
    DomainCarousel carousel;

    // Pass rvalues directly so they are moved into the CrawlTarget struct
    bool pushed = carousel.push_target({string("example.com"), string("http://example.com/1")});
    assert(pushed);

    CrawlTarget target = carousel.get_target();
    assert(target.domain == "example.com");
    assert(target.url == "http://example.com/1");
}

void test_capacity_rejection() {
    DomainCarousel carousel;
    const char* test_domain = "wikipedia.org";

    for (uint32_t i = 0; i < CRAWLER_CAROUSEL_QUEUE_SIZE; ++i) {
        // Use custom join to construct the dynamic URL
        string url = string::join("", "http://wikipedia.org/page_", string(i));
        bool pushed = carousel.push_target({string(test_domain), std::move(url)});
        assert(pushed);
    }

    bool overflow_pushed = carousel.push_target({string(test_domain), string("http://wikipedia.org/overflow")});
    assert(!overflow_pushed); // Must fail due to the fast-path size check
}

void test_politeness_delay() {
    DomainCarousel carousel;
    const char* test_domain = "slow-website.com";

    carousel.push_target({string(test_domain), string("http://slow-website.com/1")});
    carousel.push_target({string(test_domain), string("http://slow-website.com/2")});

    auto start_time = std::chrono::steady_clock::now();

    carousel.get_target();
    auto mid_time = std::chrono::steady_clock::now();

    carousel.get_target();
    auto end_time = std::chrono::steady_clock::now();

    std::chrono::duration<double> first_duration = mid_time - start_time;
    std::chrono::duration<double> second_duration = end_time - mid_time;

    assert(first_duration.count() < 1.0);
    assert(second_duration.count() >= 2.0);
}

void test_multidomain_routing() {
    DomainCarousel carousel;

    carousel.push_target({string("site-a.com"), string("http://site-a.com/1")});
    carousel.push_target({string("site-b.com"), string("http://site-b.com/1")});

    auto start_time = std::chrono::steady_clock::now();

    CrawlTarget target1 = carousel.get_target();
    CrawlTarget target2 = carousel.get_target();

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    // Use !(a == b) since operator!= is not defined in the custom string
    assert(!(target1.domain == target2.domain));
    assert(duration.count() < 1.0);
}

void test_concurrent_access() {
    DomainCarousel carousel;
    std::atomic<int> successful_pushes{0};
    std::atomic<int> successful_pops{0};

    const uint32_t num_threads = 4;
    const uint32_t items_per_thread = CRAWLER_CAROUSEL_QUEUE_SIZE / 2;

    // Pass a thread index so we can use your string(uint32_t) constructor
    auto producer = [&](uint32_t t_idx) {
        for (uint32_t i = 0; i < items_per_thread; ++i) {
            string domain = string::join("", "domain_", string(t_idx), "_", string(i), ".com");
            // Reconstruct the chars since we can't copy the `domain` string variable
            string url = string::join("", "http://domain_", string(t_idx), "_", string(i), ".com");

            if (carousel.push_target({std::move(domain), std::move(url)})) {
                successful_pushes.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    auto consumer = [&]() {
        for (uint32_t i = 0; i < items_per_thread; ++i) {
            carousel.get_target();
            successful_pops.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;

    for (uint32_t i = 0; i < num_threads; ++i) threads.emplace_back(producer, i);
    for (uint32_t i = 0; i < num_threads; ++i) threads.emplace_back(consumer);

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    assert(successful_pushes == num_threads * items_per_thread);
    assert(successful_pops == num_threads * items_per_thread);
}

int main() {
    test_basic_push_pop();
    test_capacity_rejection();
    test_multidomain_routing();
    test_politeness_delay();
    test_concurrent_access();

    std::cout << "All DomainCarousel tests passed!\n";
    return 0;
}