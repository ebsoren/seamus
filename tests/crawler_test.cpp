#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include "../crawler/crawler_listener.h"
#include "../crawler/crawler_outbound.h"
#include "../crawler/crawler_worker.h"
#include "../crawler/domain_carousel.h"


void test_crawler_listener_receives_batch() {
    printf("---- test_crawler_listener_receives_batch ----\n");

    // Construct dependencies
    vector<string> bucket_files;
    for (size_t i = 0; i < PRIORITY_BUCKETS; i++) {
        bucket_files.push_back(string("bucket_") );
    }
    DomainCarousel dc;
    BucketManager bm(static_cast<vector<string>&&>(bucket_files), &dc);

    // Start listener
    CrawlerListener cl(&bm, &dc);
    cl.start();

    // Build and send 3 batches with different targets
    BatchCrawlTargetRequest batch1;
    batch1.targets.push_back(CrawlTarget{string("example.com"), string("https://example.com/a"), 1, 0});
    batch1.targets.push_back(CrawlTarget{string("example.com"), string("https://example.com/b"), 2, 0});

    BatchCrawlTargetRequest batch2;
    batch2.targets.push_back(CrawlTarget{string("test.org"), string("http://test.org/page"), 3, 1});

    BatchCrawlTargetRequest batch3;
    batch3.targets.push_back(CrawlTarget{string("foo.net"), string("https://foo.net/x"), 0, 0});
    batch3.targets.push_back(CrawlTarget{string("bar.io"), string("https://bar.io/y"), 5, 2});
    batch3.targets.push_back(CrawlTarget{string("baz.dev"), string("https://baz.dev/z"), 10, 4});

    string host("127.0.0.1");
    assert(send_batch_crawl_target_request(host, CRAWLER_LISTENER_PORT, batch1));
    assert(send_batch_crawl_target_request(host, CRAWLER_LISTENER_PORT, batch2));
    assert(send_batch_crawl_target_request(host, CRAWLER_LISTENER_PORT, batch3));

    // Wait for the listener threads to process
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // get_priority_bucket always returns 0, so all 6 targets should be in buckets[0]
    auto& bucket = dc.buckets[0];
    std::lock_guard<std::mutex> lock(bucket.bucket_lock);
    assert(bucket.urls.size() == 6);

    // Verify all targets are present (order may vary due to thread pool)
    bool found[6] = {};
    for (size_t i = 0; i < bucket.urls.size(); i++) {
        if (bucket.urls[i].url == "https://example.com/a") { assert(bucket.urls[i].seed_distance == 1); found[0] = true; }
        if (bucket.urls[i].url == "https://example.com/b") { assert(bucket.urls[i].seed_distance == 2); found[1] = true; }
        if (bucket.urls[i].url == "http://test.org/page")   { assert(bucket.urls[i].seed_distance == 3); found[2] = true; }
        if (bucket.urls[i].url == "https://foo.net/x")      { assert(bucket.urls[i].seed_distance == 0); found[3] = true; }
        if (bucket.urls[i].url == "https://bar.io/y")        { assert(bucket.urls[i].seed_distance == 5); found[4] = true; }
        if (bucket.urls[i].url == "https://baz.dev/z")       { assert(bucket.urls[i].seed_distance == 10); found[5] = true; }
    }
    for (int i = 0; i < 6; i++) {
        assert(found[i]);
    }

    cl.stop();
    printf("PASS\n");
}


void test_push_target_fill_and_overflow() {
    printf("---- test_push_target_fill_and_overflow ----\n");

    DomainCarousel dc;
    size_t expected_slot = DomainCarousel::hash_domain(string("overflow-test.com")) % CRAWLER_CAROUSEL_SIZE;

    // Fill the slot to capacity
    for (size_t i = 0; i < CRAWLER_MAX_QUEUE_SIZE; ++i) {
        // Build a unique URL per iteration using a digit suffix
        char url_buf[64];
        snprintf(url_buf, sizeof(url_buf), "https://overflow-test.com/%zu", i);
        bool ok = dc.push_target(CrawlTarget{string("overflow-test.com"), string(url_buf, strlen(url_buf)), 0, 0});
        assert(ok);
    }

    // Verify all targets landed in the expected carousel slot
    assert(dc.carousel[expected_slot].targets.size() == CRAWLER_MAX_QUEUE_SIZE);

    // Verify other slots are empty (spot-check neighbors)
    size_t prev = (expected_slot == 0) ? CRAWLER_CAROUSEL_SIZE - 1 : expected_slot - 1;
    size_t next = (expected_slot + 1) % CRAWLER_CAROUSEL_SIZE;
    assert(dc.carousel[prev].targets.size() == 0);
    assert(dc.carousel[next].targets.size() == 0);

    // One more push should fail — queue is full
    bool overflow = dc.push_target(CrawlTarget{string("overflow-test.com"), string("https://overflow-test.com/extra"), 0, 0});
    assert(!overflow);

    // Size should still be at the max
    assert(dc.carousel[expected_slot].targets.size() == CRAWLER_MAX_QUEUE_SIZE);

    printf("PASS\n");
}


void test_feed_carousel_concurrent() {
    printf("---- test_feed_carousel_concurrent ----\n");

    DomainCarousel dc;

    // Populate two different priority buckets with targets
    // Bucket 0 (highest priority) gets 10 targets, bucket 1 gets 10 targets
    {
        std::lock_guard<std::mutex> lock0(dc.buckets[0].bucket_lock);
        for (size_t i = 0; i < 10; ++i) {
            char url_buf[64];
            snprintf(url_buf, sizeof(url_buf), "https://alpha.com/%zu", i);
            dc.buckets[0].urls.push_back(CrawlTarget{string("alpha.com"), string(url_buf, strlen(url_buf)), 0, 0});
        }
    }
    {
        std::lock_guard<std::mutex> lock1(dc.buckets[1].bucket_lock);
        for (size_t i = 0; i < 10; ++i) {
            char url_buf[64];
            snprintf(url_buf, sizeof(url_buf), "https://beta.com/%zu", i);
            dc.buckets[1].urls.push_back(CrawlTarget{string("beta.com"), string(url_buf, strlen(url_buf)), 0, 0});
        }
    }

    // Shared backoff queue for rejected targets
    std::mutex backoff_lock;
    deque<BackoffEntry> backoff_queue;

    // Call feed_carousel concurrently from two threads
    int16_t result1 = -1, result2 = -1;
    std::thread t1([&]() { result1 = dc.feed_carousel_from_highest_priority_bucket(backoff_lock, backoff_queue); });
    std::thread t2([&]() { result2 = dc.feed_carousel_from_highest_priority_bucket(backoff_lock, backoff_queue); });
    t1.join();
    t2.join();

    // Both buckets should be fully drained
    assert(dc.buckets[0].urls.size() == 0);
    assert(dc.buckets[1].urls.size() == 0);

    // At least one thread must have found a non-empty bucket
    assert(result1 >= 0 || result2 >= 0);

    // All 20 targets should be in the carousel or the backoff queue
    size_t carousel_total = 0;
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        carousel_total += dc.carousel[i].targets.size();
    }
    assert(carousel_total + backoff_queue.size() == 20);

    printf("PASS\n");
}


void test_feed_carousel_empty() {
    printf("---- test_feed_carousel_empty ----\n");

    DomainCarousel dc;

    // All buckets are empty, should return -1
    std::mutex backoff_lock;
    deque<BackoffEntry> backoff_queue;
    int16_t result = dc.feed_carousel_from_highest_priority_bucket(backoff_lock, backoff_queue);
    assert(result == -1);

    // Carousel should still be empty
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        assert(dc.carousel[i].targets.size() == 0);
    }

    printf("PASS\n");
}


void test_bucket_manager_feeds_carousel() {
    printf("---- test_bucket_manager_feeds_carousel ----\n");

    DomainCarousel dc;

    // Create bucket manager
    vector<string> bucket_files;
    for (size_t i = 0; i < PRIORITY_BUCKETS; i++) {
        bucket_files.push_back(string("bucket_"));
    }
    BucketManager bm(static_cast<vector<string>&&>(bucket_files), &dc);

    // Populate bucket 0 with 5 targets across different domains
    {
        std::lock_guard<std::mutex> lock(dc.buckets[0].bucket_lock);
        for (size_t i = 0; i < 5; ++i) {
            char url_buf[64];
            char domain_buf[64];
            snprintf(domain_buf, sizeof(domain_buf), "domain%zu.com", i);
            snprintf(url_buf, sizeof(url_buf), "https://domain%zu.com/page", i);
            dc.buckets[0].urls.push_back(CrawlTarget{
                string(domain_buf, strlen(domain_buf)),
                string(url_buf, strlen(url_buf)),
                0, 0
            });
        }
    }

    // Start the bucket manager (spawns feed thread)
    bm.start();

    // Wait for the feed thread to process
    std::this_thread::sleep_for(std::chrono::seconds(CRAWLER_FEED_INTERVAL_SEC + 1));

    // Buckets should be drained
    {
        std::lock_guard<std::mutex> lock(dc.buckets[0].bucket_lock);
        assert(dc.buckets[0].urls.size() == 0);
    }

    // All 5 targets should be in the carousel (or backoff queue if slots were full)
    size_t carousel_total = 0;
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        carousel_total += dc.carousel[i].targets.size();
    }
    {
        std::lock_guard<std::mutex> lock(bm.backoff_lock);
        assert(carousel_total + bm.backoff_queue.size() == 5);
    }

    // Verify no duplicates: total across carousel + backoff should still be 5 after another cycle
    std::this_thread::sleep_for(std::chrono::seconds(CRAWLER_FEED_INTERVAL_SEC + 1));

    size_t carousel_total_after = 0;
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        carousel_total_after += dc.carousel[i].targets.size();
    }
    {
        std::lock_guard<std::mutex> lock(bm.backoff_lock);
        assert(carousel_total_after + bm.backoff_queue.size() == 5);
    }

    printf("PASS\n");
}


void test_backoff_queue_drains_after_slot_cleared() {
    printf("---- test_backoff_queue_drains_after_slot_cleared ----\n");

    DomainCarousel dc;
    size_t total_targets = CRAWLER_MAX_QUEUE_SIZE + 10;
    size_t overflow_count = 10;

    vector<string> bucket_files;
    for (size_t i = 0; i < PRIORITY_BUCKETS; i++) {
        bucket_files.push_back(string("bucket_"));
    }
    BucketManager bm(static_cast<vector<string>&&>(bucket_files), &dc);

    // Push more targets to the same domain than the carousel slot can hold
    {
        std::lock_guard<std::mutex> lock(dc.buckets[0].bucket_lock);
        for (size_t i = 0; i < total_targets; ++i) {
            char url_buf[64];
            snprintf(url_buf, sizeof(url_buf), "https://same-domain.com/%zu", i);
            dc.buckets[0].urls.push_back(CrawlTarget{
                string("same-domain.com"),
                string(url_buf, strlen(url_buf)),
                0, 0
            });
        }
    }

    bm.start();

    // Wait for the feed thread to drain the bucket into the carousel + backoff
    std::this_thread::sleep_for(std::chrono::seconds(CRAWLER_FEED_INTERVAL_SEC + 1));

    // Bucket should be drained
    {
        std::lock_guard<std::mutex> lock(dc.buckets[0].bucket_lock);
        assert(dc.buckets[0].urls.size() == 0);
    }

    // Carousel slot should be full, overflow should be in backoff queue
    size_t slot = DomainCarousel::hash_domain(string("same-domain.com")) % CRAWLER_CAROUSEL_SIZE;
    assert(dc.carousel[slot].targets.size() == CRAWLER_MAX_QUEUE_SIZE);
    {
        std::lock_guard<std::mutex> lock(bm.backoff_lock);
        assert(bm.backoff_queue.size() == overflow_count);
    }

    // Clear the carousel slot to make room for backoff items
    {
        std::lock_guard<std::mutex> lock(dc.carousel[slot].domain_queue_lock);
        while (!dc.carousel[slot].targets.empty()) {
            dc.carousel[slot].targets.pop_front();
        }
    }

    // Wait for backoff period (CRAWLER_BACKOFF_SEC) + a feed cycle to process them
    std::this_thread::sleep_for(std::chrono::seconds(CRAWLER_BACKOFF_SEC + CRAWLER_FEED_INTERVAL_SEC + 1));

    // Backoff queue should now be empty — all overflow items moved to carousel
    {
        std::lock_guard<std::mutex> lock(bm.backoff_lock);
        assert(bm.backoff_queue.size() == 0);
    }

    // The overflow items should now be in the carousel slot
    assert(dc.carousel[slot].targets.size() == overflow_count);

    printf("PASS\n");
}


void test_bucket_manager_empty_buckets() {
    printf("---- test_bucket_manager_empty_buckets ----\n");

    DomainCarousel dc;

    vector<string> bucket_files;
    for (size_t i = 0; i < PRIORITY_BUCKETS; i++) {
        bucket_files.push_back(string("bucket_"));
    }
    BucketManager bm(static_cast<vector<string>&&>(bucket_files), &dc);

    // Start with all buckets empty
    bm.start();

    // Let the feed thread run a couple cycles
    std::this_thread::sleep_for(std::chrono::seconds(CRAWLER_FEED_INTERVAL_SEC + 1));

    // Carousel should remain completely empty
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        assert(dc.carousel[i].targets.size() == 0);
    }

    // Backoff queue should also be empty
    {
        std::lock_guard<std::mutex> lock(bm.backoff_lock);
        assert(bm.backoff_queue.size() == 0);
    }

    printf("PASS\n");
}


void test_spawn_crawler_workers_consumes_and_stops() {
    printf("---- test_spawn_crawler_workers_consumes_and_stops ----\n");

    DomainCarousel dc;
    std::atomic<bool> running{true};

    // Populate every carousel slot directly with one target
    // Use 127.0.0.1 as domain so https_get fails instantly (connection refused on port 443)
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        char url_buf[64];
        snprintf(url_buf, sizeof(url_buf), "https://127.0.0.1/page%zu", i);
        std::lock_guard<std::mutex> lock(dc.carousel[i].domain_queue_lock);
        dc.carousel[i].targets.push_back(CrawlTarget{
            string("127.0.0.1"),
            string(url_buf, strlen(url_buf)),
            0, 0
        });
    }

    // Verify all slots are populated
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        assert(dc.carousel[i].targets.size() == 1);
    }

    // Spawn workers
    spawn_crawler_workers(dc, running);

    // Wait for workers to consume all targets
    std::this_thread::sleep_for(std::chrono::seconds(CRAWLER_BACKOFF_SEC + 3));

    // All slots should be empty
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        std::lock_guard<std::mutex> lock(dc.carousel[i].domain_queue_lock);
        assert(dc.carousel[i].targets.size() == 0);
    }

    // Shut down workers
    running.store(false, std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Repopulate every slot
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        char url_buf[64];
        snprintf(url_buf, sizeof(url_buf), "https://127.0.0.1/page2_%zu", i);
        std::lock_guard<std::mutex> lock(dc.carousel[i].domain_queue_lock);
        dc.carousel[i].targets.push_back(CrawlTarget{
            string("127.0.0.1"),
            string(url_buf, strlen(url_buf)),
            0, 0
        });
    }

    // Wait and verify targets are NOT consumed (workers are stopped)
    std::this_thread::sleep_for(std::chrono::seconds(CRAWLER_BACKOFF_SEC + 3));

    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        std::lock_guard<std::mutex> lock(dc.carousel[i].domain_queue_lock);
        assert(dc.carousel[i].targets.size() == 1);
    }

    printf("PASS\n");
}


void test_crawler_outbound_flushes_to_listener() {
    printf("---- test_crawler_outbound_flushes_to_listener ----\n");

    DomainCarousel dc;

    vector<string> bucket_files;
    for (size_t i = 0; i < PRIORITY_BUCKETS; i++) {
        bucket_files.push_back(string("bucket_"));
    }
    BucketManager bm(static_cast<vector<string>&&>(bucket_files), &dc);

    // Start the crawler listener (machine 0 = 127.0.0.1)
    CrawlerListener cl(&bm, &dc);
    cl.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Push exactly CRAWLER_OUTBOUND_BATCH_SIZE targets to machine 0
    // This should trigger a flush to the local crawler listener
    CrawlerOutbound outbound;
    for (size_t i = 0; i < CRAWLER_OUTBOUND_BATCH_SIZE; ++i) {
        char url_buf[64];
        snprintf(url_buf, sizeof(url_buf), "https://outbound%zu.com/page", i);
        char domain_buf[64];
        snprintf(domain_buf, sizeof(domain_buf), "outbound%zu.com", i);
        outbound.add(0, CrawlTarget{
            string(domain_buf, strlen(domain_buf)),
            string(url_buf, strlen(url_buf)),
            0, 0
        });
    }

    // Wait for the listener to process the batch
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Verify total targets across all priority buckets equals CRAWLER_OUTBOUND_BATCH_SIZE
    size_t total = 0;
    for (size_t i = 0; i < PRIORITY_BUCKETS; ++i) {
        std::lock_guard<std::mutex> lock(dc.buckets[i].bucket_lock);
        total += dc.buckets[i].urls.size();
    }
    assert(total == CRAWLER_OUTBOUND_BATCH_SIZE);

    cl.stop();
    printf("PASS\n");
}


int main() {
    printf("\n===== RUNNING CRAWLER TESTS =====\n\n");
    test_crawler_listener_receives_batch();
    test_push_target_fill_and_overflow();
    test_feed_carousel_concurrent();
    test_feed_carousel_empty();
    test_bucket_manager_feeds_carousel();
    test_backoff_queue_drains_after_slot_cleared();
    test_bucket_manager_empty_buckets();
    test_spawn_crawler_workers_consumes_and_stops();
    test_crawler_outbound_flushes_to_listener();
    printf("\n===== ALL CRAWLER TESTS PASSED =====\n");
    return 0;
}
