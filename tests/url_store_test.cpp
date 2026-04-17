#include "../url_store/url_store.h"
#include "../crawler/domain_carousel.h"
#include "../lib/rpc_urlstore.h"
#include "../lib/consts.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

using std::cout;
using std::endl;

const int URL_STORE_WORKER_NUMBER = 0;

// Helper to clean up the test file so tests don't pollute each other across runs
void cleanup_test_file(int worker_number) {
    string fileName = string::join("", URL_STORE_OUTPUT_DIR_STR, "/urlstore.txt");
    remove(fileName.data());
}

// Tests getters, setters for class data structures as well as content title and body detection
void test_url_store_basic() {
    cout << string("Running test_url_store_basic...") << endl;
    cleanup_test_file(URL_STORE_WORKER_NUMBER);
    UrlStore store(nullptr, URL_STORE_WORKER_NUMBER);
    
    string umich_url("https://umich.edu");
    string dne_url("https://doesnotexist.com");
    
    // Test 1: Adding a new URL
    vector<string> anchors1;
    anchors1.push_back(string("michigan"));
    anchors1.push_back(string("home"));
    
    bool added = store.updateUrl(umich_url, anchors1, 2, 1, 1);
    store.markCrawled(umich_url); // Mark as crawled
    assert(added == true);

    string title("test_title");
    store.updateTitleLen(umich_url, 5);
    store.updateTitle(umich_url, title);
    store.updateBodyLen(umich_url, 15);
    
    // Verify basic getters
    assert(store.getUrlNumEncountered(umich_url) == 1);
    assert(store.getUrlSeedDistance(umich_url) == 2);
    
    // Verify text positioning
    assert(store.getTitle(umich_url) == "test_title");

    // Test 2: Updating an existing URL
    vector<string> anchors2;
    anchors2.push_back(string("michigan")); 
    anchors2.push_back(string("university")); 
    
    bool is_new = store.updateUrl(umich_url, anchors2, 1, 1, 3);
    store.markCrawled(umich_url); // Mark as crawled
    assert(is_new == false);
    
    // Verify counters and minimum distances updated properly
    assert(store.getUrlNumEncountered(umich_url) == 4); 
    assert(store.getUrlSeedDistance(umich_url) == 1);   

    // Verify Anchor Data
    auto anchor_info = store.getUrlAnchorInfo(umich_url);
    assert(anchor_info.size() == 3); 
    
    string michigan_anchor("michigan"); 
    for (const auto& a : anchor_info) {
        if (*(a.anchor_text) == michigan_anchor) {
            assert(a.freq == 2);
        } else {
            assert(a.freq == 1);
        }
    }

    cout << string("-> Passed test_url_store_basic\n") << endl;
}

void test_url_store_persistence() {
    cout << string("Running test_url_store_persistence...") << endl;
    cleanup_test_file(URL_STORE_WORKER_NUMBER);

    UrlStore store(nullptr, URL_STORE_WORKER_NUMBER);
    vector<string> anchors;
    anchors.push_back(string("persisted link"));
    
    string persist_url("http://persist.me");
    store.updateUrl(persist_url, anchors, 5, 2, 42);
    store.markCrawled(persist_url); // Mark as crawled so it survives destruction
    
    store.updateTitleLen(persist_url, 10);
    store.updateBodyLen(persist_url, 25);
    
    store.persist();
    
    // Basic sanity check that the file can be opened
    string fileName = string::join("", URL_STORE_OUTPUT_DIR_STR, "/urlstore.txt");
    string read_mode("r");
    FILE* fd = fopen(fileName.data(), read_mode.data());
    assert(fd != nullptr);
    fclose(fd);

    cout << string("-> Passed test_url_store_persistence\n") << endl;
}

void test_url_store_recover() {
    cout << string("Running test_url_store_recover...") << endl;
    UrlStore store(nullptr, URL_STORE_WORKER_NUMBER);

    string persist_url("http://persist.me");
    string persist_anchor("persisted link");
    
    // Mark as crawled to ensure safe destruction of this local instance too
    store.markCrawled(persist_url);

    assert(store.getUrlNumEncountered(persist_url) == 42);
    assert(store.getUrlSeedDistance(persist_url) == 5);
    
    auto anchor_info = store.getUrlAnchorInfo(persist_url);
    assert(anchor_info.size() == 1);
    assert(*(anchor_info[0].anchor_text) == persist_anchor);
    assert(anchor_info[0].freq == 1);

    cleanup_test_file(URL_STORE_WORKER_NUMBER);
    cout << string("-> Passed test_url_store_recover\n") << endl;
}

void test_url_store_listener_test() {
    cout << string("Running test_url_store_listener_test...") << endl;
    
    UrlStore store(nullptr, URL_STORE_WORKER_NUMBER); 
    
    BatchURLStoreUpdateRequest batch;
    URLStoreUpdateRequest req;
    
    req.url = string("http://rpc-test.com");
    req.anchor_text.push_back(string("rpc anchor"));
    req.num_encountered = 1;
    req.seed_list_url_hops = 3;
    req.seed_list_domain_hops = 3;
    batch.reqs.push_back(::move(req));

    string local_ip("127.0.0.1");
    bool send_success = send_batch_urlstore_update(local_ip, URL_STORE_PORT, batch);
    assert(send_success == true);

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    string rpc_url("http://rpc-test.com");
    string rpc_anchor("rpc anchor");

    // Mark as crawled after the listener thread has parsed it
    store.markCrawled(rpc_url);

    assert(store.getUrlNumEncountered(rpc_url) == 1);
    assert(store.getUrlSeedDistance(rpc_url) == 3);
    
    auto anchor_info = store.getUrlAnchorInfo(rpc_url);
    assert(anchor_info.size() == 1);
    assert(*(anchor_info[0].anchor_text) == rpc_anchor);

    cout << string("-> Passed test_url_store_listener_test\n") << endl;
}

void test_url_store_concurrent_same_url() {
    cout << string("Running test_url_store_concurrent_same_url...") << endl;
    UrlStore store(nullptr, URL_STORE_WORKER_NUMBER);
    const int num_threads = 10;
    const int updates_per_thread = 1000;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread([&store]() {
            string url("http://viral-page.com");
            vector<string> anchors;
            anchors.push_back(string("click here"));

            for (int j = 0; j < updates_per_thread; ++j) {
                store.updateUrl(url, anchors, 2, 2, 1);
                store.markCrawled(url); // Mark as crawled
            }
        }));
    }

    for (auto& t : threads) t.join();

    string target_url("http://viral-page.com");
    uint32_t expected_encounters = num_threads * updates_per_thread;
    assert(store.getUrlNumEncountered(target_url) == expected_encounters);

    cout << string("-> Passed test_url_store_concurrent_same_url\n") << endl;
}

void test_url_store_concurrent_different_urls() {
    cout << string("Running test_url_store_concurrent_different_urls...") << endl;
    UrlStore store(nullptr, URL_STORE_WORKER_NUMBER);
    const int num_threads = 50;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread([&store, i]() {
            string url(string::join("", "http://node-", string(i), ".com"));
            vector<string> anchors;
            anchors.push_back(string("unique link"));
            store.updateUrl(url, anchors, 1, 1, 1);
            store.markCrawled(url); // Mark as crawled
        }));
    }

    for (auto& t : threads) t.join();

    for (int i = 0; i < num_threads; ++i) {
        string url(string::join("", "http://node-", string(i), ".com"));
        assert(store.getUrlNumEncountered(url) == 1);
    }

    cout << string("-> Passed test_url_store_concurrent_different_urls\n") << endl;
}

void test_url_store_concurrent_anchors() {
    cout << string("Running test_url_store_concurrent_anchors...") << endl;
    UrlStore store(nullptr, URL_STORE_WORKER_NUMBER);
    const int num_threads = 20;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread([&store, i]() {
            string url("http://anchor-aggregator.com");
            vector<string> anchors;
            anchors.push_back(string("shared anchor"));
            anchors.push_back(string::join("", "unique anchor ", string(i)));
            
            store.updateUrl(url, anchors, 1, 1, 1);
            store.markCrawled(url); // Mark as crawled
        }));
    }

    for (auto& t : threads) t.join();

    string target_url("http://anchor-aggregator.com");
    auto anchor_info = store.getUrlAnchorInfo(target_url);
    
    assert(anchor_info.size() == num_threads + 1);

    bool found_shared = false;
    string shared_str("shared anchor");
    
    for (const auto& a : anchor_info) {
        if (*(a.anchor_text) == shared_str) {
            assert(a.freq == num_threads);
            found_shared = true;
        } else {
            assert(a.freq == 1);
        }
    }
    assert(found_shared == true);

    cout << string("-> Passed test_url_store_concurrent_anchors\n") << endl;
}

void test_has_url() {
    cout << string("Running test_has_url...") << endl;
    UrlStore store(nullptr, URL_STORE_WORKER_NUMBER);

    string url1("https://example.com/page1");
    string url2("https://example.com/page2");

    assert(!store.hasUrl(url1));
    assert(!store.hasUrl(url2));

    vector<string> anchors;
    anchors.push_back(string("link text"));
    store.updateUrl(url1, anchors, 1, 0, 1);
    store.markCrawled(url1); // Mark as crawled

    assert(store.hasUrl(url1));
    assert(!store.hasUrl(url2));

    vector<string> anchors2;
    anchors2.push_back(string("other text"));
    store.updateUrl(url1, anchors2, 2, 1, 1);
    store.markCrawled(url1); // Mark as crawled
    assert(store.hasUrl(url1));

    cout << string("-> Passed test_has_url\n") << endl;
}

void test_url_store_massive_stress() {
    cout << string("Running test_url_store_massive_stress (This might take a second)...") << endl;
    UrlStore store(nullptr, URL_STORE_WORKER_NUMBER);
    
    const int num_threads = 32;
    const int ops_per_thread = 5000;
    
    vector<std::thread> threads;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread([&store, i, ops_per_thread]() {
            string shared_url("http://the-viral-page.com");
            string shared_anchor("clickbait");
            
            for (uint32_t j = 0; j < ops_per_thread; ++j) {
                vector<string> shared_anchors;
                shared_anchors.push_back(string(shared_anchor.data(), shared_anchor.size()));
                store.updateUrl(shared_url, shared_anchors, 2, 2, 1);
                store.markCrawled(shared_url); // Mark as crawled
                
                string unique_url = string::join("", "http://unique-", string(i), "-", string(j), ".com");
                string unique_anchor = string::join("", "anchor-", string(i), "-", string(j));
                
                vector<string> unique_anchors;
                unique_anchors.push_back(string(unique_anchor.data(), unique_anchor.size()));
                
                if (j % 2 == 0) unique_anchors.push_back(string(shared_anchor.data(), shared_anchor.size()));
                
                store.updateUrl(unique_url, unique_anchors, 5, 5, 1);
                store.markCrawled(unique_url); // Mark as crawled
            }
        }));
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end_time - start_time;
    cout << "Stress test completed in " << diff.count() << " seconds." << endl;
    
    cout << string("Verifying memory integrity and mathematical correctness...") << endl;
    
    string shared_url("http://the-viral-page.com");
    uint32_t expected_shared_encounters = num_threads * ops_per_thread;
    uint32_t actual_encounters = store.getUrlNumEncountered(shared_url);
    
    if (actual_encounters != expected_shared_encounters) {
        cout << string("FAILURE: Race condition detected! Expected ") 
             << expected_shared_encounters << string(" but got ") << actual_encounters << endl;
    }
    assert(actual_encounters == expected_shared_encounters);
    
    string sample_unique_1 = string::join("", "http://unique-", string(static_cast<uint32_t>(0)), "-", string(ops_per_thread / 2), ".com");
    string sample_unique_2 = string::join("", "http://unique-", string(num_threads - 1), "-", string(ops_per_thread - 1), ".com");
    
    assert(store.getUrlNumEncountered(sample_unique_1) == 1);
    assert(store.getUrlNumEncountered(sample_unique_2) == 1);
    
    cout << string("-> Passed test_url_store_massive_stress\n") << endl;
}

int main() {
    cout << string("Starting UrlStore Test Suite...\n") << endl;

    test_url_store_basic();
    test_url_store_persistence();
    test_url_store_recover();
    test_url_store_listener_test();
    test_url_store_concurrent_same_url();
    test_url_store_concurrent_different_urls();
    test_url_store_concurrent_anchors();
    test_has_url();
    test_url_store_massive_stress();

    cout << string("All UrlStore tests passed successfully!") << endl;
    return 0;
}