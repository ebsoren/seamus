#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include "../crawler/crawler_listener.h"
#include "../crawler/domain_carousel.h"


void test_crawler_listener_receives_batch() {
    printf("---- test_crawler_listener_receives_batch ----\n");

    // Construct dependencies
    vector<string> bucket_files;
    for (size_t i = 0; i < PRIORITY_BUCKETS; i++) {
        bucket_files.push_back(string("bucket_") );
    }
    BucketManager bm(static_cast<vector<string>&&>(bucket_files));
    DomainCarousel dc;

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

    // Call feed_carousel concurrently from two threads
    int16_t result1 = -1, result2 = -1;
    std::thread t1([&]() { result1 = dc.feed_carousel(); });
    std::thread t2([&]() { result2 = dc.feed_carousel(); });
    t1.join();
    t2.join();

    // Both buckets should be fully drained
    assert(dc.buckets[0].urls.size() == 0);
    assert(dc.buckets[1].urls.size() == 0);

    // At least one thread must have found a non-empty bucket
    assert(result1 >= 0 || result2 >= 0);

    // Count total targets across all carousel slots
    size_t total = 0;
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        total += dc.carousel[i].targets.size();
    }
    assert(total == 20);

    printf("PASS\n");
}


void test_feed_carousel_empty() {
    printf("---- test_feed_carousel_empty ----\n");

    DomainCarousel dc;

    // All buckets are empty, should return -1
    int16_t result = dc.feed_carousel();
    assert(result == -1);

    // Carousel should still be empty
    for (size_t i = 0; i < CRAWLER_CAROUSEL_SIZE; ++i) {
        assert(dc.carousel[i].targets.size() == 0);
    }

    printf("PASS\n");
}


int main() {
    printf("\n===== RUNNING CRAWLER TESTS =====\n\n");
    test_crawler_listener_receives_batch();
    test_push_target_fill_and_overflow();
    test_feed_carousel_concurrent();
    test_feed_carousel_empty();
    printf("\n===== ALL CRAWLER TESTS PASSED =====\n");
    return 0;
}
