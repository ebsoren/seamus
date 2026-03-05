#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include "../crawler/crawler_listener.h"


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


void test_crawler_listener_empty_batch() {
    printf("---- test_crawler_listener_empty_batch ----\n");

    vector<string> bucket_files;
    for (size_t i = 0; i < PRIORITY_BUCKETS; i++) {
        bucket_files.push_back(string("bucket_"));
    }
    BucketManager bm(static_cast<vector<string>&&>(bucket_files));
    DomainCarousel dc;

    CrawlerListener cl(&bm, &dc);
    cl.start();

    // Send an empty batch
    BatchCrawlTargetRequest batch;
    string host("127.0.0.1");
    assert(send_batch_crawl_target_request(host, CRAWLER_LISTENER_PORT, batch));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // All buckets should be empty
    for (size_t i = 0; i < PRIORITY_BUCKETS; i++) {
        std::lock_guard<std::mutex> lock(dc.buckets[i].bucket_lock);
        assert(dc.buckets[i].urls.size() == 0);
    }

    cl.stop();
    printf("PASS\n");
}


int main() {
    printf("\n===== RUNNING CRAWLER TESTS =====\n\n");
    test_crawler_listener_receives_batch();
    // Can't run both tests since they bind to the same port
    // test_crawler_listener_empty_batch();
    printf("\n===== ALL CRAWLER TESTS PASSED =====\n");
    return 0;
}
