#include "../url_store/url_store.h"
#include "../crawler/domain_carousel.h"
#include "../lib/rpc_urlstore.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

using std::cout;
using std::endl;

// Helper to clean up the test file so tests don't pollute each other across runs
void cleanup_test_file(int worker_number) {
    // join natively accepts raw C-string literals, so this remains safe
    string fileName = string::join("urlstore_", string(worker_number), ".txt");
    remove(fileName.data());
}

// Tests getters, setters for class data structures as well as content title and body detection
void test_url_store_basic() {
    cout << string("Running test_url_store_basic...") << endl;
    UrlStore store(nullptr);
    
    // Workaround 1: Create explicit lvalue variables for the URLs
    string umich_url("https://umich.edu");
    string dne_url("https://doesnotexist.com");
    
    // Test 1: Adding a new URL
    vector<string> anchors1;
    anchors1.push_back(string("michigan")); // Keep as rvalue to allow move semantics
    anchors1.push_back(string("home"));
    
    bool added = store.addUrl(umich_url, anchors1, 2, 1, 5, 15, 1);
    assert(added == true);
    
    // Verify basic getters
    assert(store.getUrlNumEncountered(umich_url) == 1);
    assert(store.getUrlSeedDistance(umich_url) == 2);
    
    // Verify text positioning
    assert(store.inTitle(umich_url, 3) == true);   // 3 < eot (5)
    assert(store.inTitle(umich_url, 6) == false);  // 6 > eot (5)
    assert(store.inDescription(umich_url, 10) == true); // 5 <= 10 < eod (15)
    assert(store.inDescription(umich_url, 20) == false); // 20 > eod (15)

    // Test 2: Updating an existing URL
    vector<string> anchors2;
    anchors2.push_back(string("michigan")); 
    anchors2.push_back(string("university")); 
    
    bool updated = store.updateUrl(umich_url, anchors2, 1, 1, 3);
    assert(updated == true);
    
    // Verify counters and minimum distances updated properly
    assert(store.getUrlNumEncountered(umich_url) == 4); 
    assert(store.getUrlSeedDistance(umich_url) == 1);   

    // Verify Anchor Data
    auto anchor_info = store.getUrlAnchorInfo(umich_url);
    assert(anchor_info.size() == 3); 
    
    // Ensure duplicate anchors added correctly
    string michigan_anchor("michigan"); // Lvalue for comparison
    for (const auto& a : anchor_info) {
        if (*(a.anchor_text) == michigan_anchor) {
            assert(a.freq == 2);
        } else {
            assert(a.freq == 1);
        }
    }
    
    // Test 3: Updating a non-existent URL should fail
    bool update_calls_add = store.updateUrl(dne_url, anchors1, 1, 1, 1);
    assert(update_calls_add == true);

    bool dup_url = store.addUrl(umich_url, anchors1, 0, 0, 0, 0, 0);
    assert(dup_url == false);

    cout << string("-> Passed test_url_store_basic\n") << endl;
}

void test_url_store_persistence() {
    cout << string("Running test_url_store_persistence...") << endl;
    cleanup_test_file(URL_STORE_WORKER_NUMBER);

    UrlStore store(nullptr);
    vector<string> anchors;
    anchors.push_back(string("persisted link"));
    
    string persist_url("http://persist.me");
    store.addUrl(persist_url, anchors, 5, 2, 10, 20, 42);
    
    // This should write to urlstore_0_tmp.txt and rename to urlstore_0.txt
    store.persist();
    
    // Basic sanity check that the file can be opened
    string fileName = string::join("urlstore_", string(URL_STORE_WORKER_NUMBER), ".txt");
    string read_mode("r");
    FILE* fd = fopen(fileName.data(), read_mode.data());
    assert(fd != nullptr);
    fclose(fd);

    cout << string("-> Passed test_url_store_persistence\n") << endl;
}

void test_url_store_recover() {
    cout << string("Running test_url_store_recover...") << endl;
    UrlStore store(nullptr); // Fresh instance, empty in memory
    
    // Read from the file we just persisted
    UrlStore::readFromFile(store, URL_STORE_WORKER_NUMBER);
    
    string persist_url("http://persist.me");
    string persist_anchor("persisted link");
    
    // Validate the parsed data matches exactly what we injected in the previous test
    assert(store.getUrlNumEncountered(persist_url) == 42);
    assert(store.getUrlSeedDistance(persist_url) == 5);
    assert(store.inTitle(persist_url, 5) == true);
    assert(store.inDescription(persist_url, 15) == true);
    
    auto anchor_info = store.getUrlAnchorInfo(persist_url);
    assert(anchor_info.size() == 1);
    assert(*(anchor_info[0].anchor_text) == persist_anchor);
    assert(anchor_info[0].freq == 1);

    cleanup_test_file(URL_STORE_WORKER_NUMBER); // Clean up after ourselves
    cout << string("-> Passed test_url_store_recover\n") << endl;
}

void test_url_store_listener_test() {
    cout << string("Running test_url_store_listener_test...") << endl;
    
    // Spinning up this object will automatically start the background RPCListener thread on URL_STORE_PORT 9000
    UrlStore store(nullptr); 
    
    // Build a mock network request simulating a worker finding a new URL
    BatchURLStoreUpdateRequest batch;
    URLStoreUpdateRequest req;
    
    // Keep these as rvalues so the struct can invoke move-assignment
    req.url = string("http://rpc-test.com");
    req.anchor_text.push_back(string("rpc anchor"));
    req.num_encountered = 1;
    req.seed_list_url_hops = 3;
    req.seed_list_domain_hops = 3;
    batch.reqs.push_back(::move(req));

    string local_ip("127.0.0.1");
    bool send_success = send_batch_urlstore_update(local_ip, URL_STORE_PORT, batch);
    assert(send_success == true);

    // Give the listener thread a brief moment to accept the socket, read, and mutate state
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    string rpc_url("http://rpc-test.com");
    string rpc_anchor("rpc anchor");

    // Verify the background thread correctly invoked client_handler and mutated our UrlStore
    assert(store.getUrlNumEncountered(rpc_url) == 1);
    assert(store.getUrlSeedDistance(rpc_url) == 3);
    
    auto anchor_info = store.getUrlAnchorInfo(rpc_url);
    assert(anchor_info.size() == 1);
    assert(*(anchor_info[0].anchor_text) == rpc_anchor);

    cout << string("-> Passed test_url_store_listener_test\n") << endl;
}

void test_manage_frontier_via_listener() {
    cout << string("Running test_manage_frontier_via_listener...") << endl;

    DomainCarousel dc;
    UrlStore store(&dc);

    // Send a batch update over the network to the listener
    BatchURLStoreUpdateRequest batch;
    URLStoreUpdateRequest req;
    req.url = string("https://www.listener-frontier.com/page");
    req.anchor_text.push_back(string("listener test"));
    req.num_encountered = 1;
    req.seed_list_url_hops = 4;
    req.seed_list_domain_hops = 2;
    batch.reqs.push_back(::move(req));

    string local_ip("127.0.0.1");
    bool sent = send_batch_urlstore_update(local_ip, URL_STORE_PORT, batch);
    assert(sent == true);

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // Verify URL was added to the store
    string check_url("https://www.listener-frontier.com/page");
    assert(store.getUrlNumEncountered(check_url) == 1);
    assert(store.getUrlSeedDistance(check_url) == 4);

    // Verify the listener path also pushed a CrawlTarget to the frontier
    assert(dc.buckets[0].urls.size() == 1);
    string expected_domain("listener-frontier.com");
    assert(dc.buckets[0].urls.front().domain == expected_domain);
    assert(dc.buckets[0].urls.front().seed_distance == 4);
    assert(dc.buckets[0].urls.front().domain_dist == 2);

    // Send the same URL again — should update store but NOT add to frontier
    BatchURLStoreUpdateRequest batch2;
    URLStoreUpdateRequest req2;
    req2.url = string("https://www.listener-frontier.com/page");
    req2.anchor_text.push_back(string("another anchor"));
    req2.num_encountered = 1;
    req2.seed_list_url_hops = 1;
    req2.seed_list_domain_hops = 0;
    batch2.reqs.push_back(::move(req2));

    sent = send_batch_urlstore_update(local_ip, URL_STORE_PORT, batch2);
    assert(sent == true);

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    assert(store.getUrlNumEncountered(check_url) == 2);
    assert(store.getUrlSeedDistance(check_url) == 1);
    assert(dc.buckets[0].urls.size() == 1);

    cout << string("-> Passed test_manage_frontier_via_listener\n") << endl;
}

void test_manage_frontier_and_update_url() {
    cout << string("Running test_manage_frontier_and_update_url...") << endl;

    DomainCarousel dc;
    UrlStore store(&dc);

    // New URL should be added to the url store AND pushed to a frontier bucket
    URLStoreUpdateRequest req;
    req.url = string("https://www.example.com/page1");
    req.anchor_text.push_back(string("example"));
    req.num_encountered = 1;
    req.seed_list_url_hops = 2;
    req.seed_list_domain_hops = 1;

    store.manage_frontier_and_update_url(req);

    // Verify URL was added to the store
    string check_url("https://www.example.com/page1");
    assert(store.getUrlNumEncountered(check_url) == 1);
    assert(store.getUrlSeedDistance(check_url) == 2);

    // Verify a CrawlTarget was pushed to bucket 0
    assert(dc.buckets[0].urls.size() == 1);
    string expected_domain("example.com");
    assert(dc.buckets[0].urls.front().domain == expected_domain);
    assert(dc.buckets[0].urls.front().seed_distance == 2);
    assert(dc.buckets[0].urls.front().domain_dist == 1);

    // Second update for the same URL should NOT push another target to the frontier
    URLStoreUpdateRequest req2;
    req2.url = string("https://www.example.com/page1");
    req2.anchor_text.push_back(string("example site"));
    req2.num_encountered = 1;
    req2.seed_list_url_hops = 1;
    req2.seed_list_domain_hops = 0;

    store.manage_frontier_and_update_url(req2);

    // Store should be updated (encounter count increased, seed distance lowered)
    assert(store.getUrlNumEncountered(check_url) == 2);
    assert(store.getUrlSeedDistance(check_url) == 1);

    // But frontier should still have only one target
    assert(dc.buckets[0].urls.size() == 1);

    cout << string("-> Passed test_manage_frontier_and_update_url\n") << endl;
}

void test_batch_manage_frontier_and_update_url() {
    cout << string("Running test_batch_manage_frontier_and_update_url...") << endl;

    DomainCarousel dc;
    UrlStore store(&dc);

    // Build a batch with 3 distinct new URLs and 1 duplicate
    BatchURLStoreUpdateRequest batch;

    URLStoreUpdateRequest req1;
    req1.url = string("https://www.alpha.com/page");
    req1.anchor_text.push_back(string("alpha"));
    req1.num_encountered = 1;
    req1.seed_list_url_hops = 2;
    req1.seed_list_domain_hops = 1;
    batch.reqs.push_back(::move(req1));

    URLStoreUpdateRequest req2;
    req2.url = string("https://www.beta.com/page");
    req2.anchor_text.push_back(string("beta"));
    req2.num_encountered = 1;
    req2.seed_list_url_hops = 3;
    req2.seed_list_domain_hops = 2;
    batch.reqs.push_back(::move(req2));

    URLStoreUpdateRequest req3;
    req3.url = string("https://www.gamma.com/page");
    req3.anchor_text.push_back(string("gamma"));
    req3.num_encountered = 1;
    req3.seed_list_url_hops = 1;
    req3.seed_list_domain_hops = 0;
    batch.reqs.push_back(::move(req3));

    // Duplicate of alpha — should update store but NOT add a second frontier entry
    URLStoreUpdateRequest req4;
    req4.url = string("https://www.alpha.com/page");
    req4.anchor_text.push_back(string("alpha link"));
    req4.num_encountered = 1;
    req4.seed_list_url_hops = 5;
    req4.seed_list_domain_hops = 3;
    batch.reqs.push_back(::move(req4));

    store.batch_manage_frontier_and_update_url(batch);

    // Verify all 3 distinct URLs are in the store
    string alpha_url("https://www.alpha.com/page");
    string beta_url("https://www.beta.com/page");
    string gamma_url("https://www.gamma.com/page");

    assert(store.getUrlNumEncountered(alpha_url) == 2);  // encountered twice in batch
    assert(store.getUrlSeedDistance(alpha_url) == 2);     // min(2, 5) = 2
    assert(store.getUrlNumEncountered(beta_url) == 1);
    assert(store.getUrlSeedDistance(beta_url) == 3);
    assert(store.getUrlNumEncountered(gamma_url) == 1);
    assert(store.getUrlSeedDistance(gamma_url) == 1);

    // Verify exactly 3 CrawlTargets were pushed to the frontier (not 4)
    size_t total_frontier = 0;
    for (size_t i = 0; i < PRIORITY_BUCKETS; ++i) {
        std::lock_guard<std::mutex> lock(dc.buckets[i].bucket_lock);
        total_frontier += dc.buckets[i].urls.size();
    }
    assert(total_frontier == 3);

    // Verify frontier entries have correct data (all in bucket 0 with current hardcoded priority)
    assert(dc.buckets[0].urls.size() == 3);

    string expected_alpha("alpha.com");
    string expected_beta("beta.com");
    string expected_gamma("gamma.com");

    // Targets should be in insertion order
    assert(dc.buckets[0].urls[0].domain == expected_alpha);
    assert(dc.buckets[0].urls[0].seed_distance == 2);
    assert(dc.buckets[0].urls[0].domain_dist == 1);

    assert(dc.buckets[0].urls[1].domain == expected_beta);
    assert(dc.buckets[0].urls[1].seed_distance == 3);
    assert(dc.buckets[0].urls[1].domain_dist == 2);

    assert(dc.buckets[0].urls[2].domain == expected_gamma);
    assert(dc.buckets[0].urls[2].seed_distance == 1);
    assert(dc.buckets[0].urls[2].domain_dist == 0);

    // Now call batch again with one new URL and one existing — only the new one should hit frontier
    BatchURLStoreUpdateRequest batch2;

    URLStoreUpdateRequest req5;
    req5.url = string("https://www.delta.com/page");
    req5.anchor_text.push_back(string("delta"));
    req5.num_encountered = 1;
    req5.seed_list_url_hops = 1;
    req5.seed_list_domain_hops = 1;
    batch2.reqs.push_back(::move(req5));

    URLStoreUpdateRequest req6;
    req6.url = string("https://www.beta.com/page");
    req6.anchor_text.push_back(string("beta again"));
    req6.num_encountered = 1;
    req6.seed_list_url_hops = 1;
    req6.seed_list_domain_hops = 1;
    batch2.reqs.push_back(::move(req6));

    store.batch_manage_frontier_and_update_url(batch2);

    assert(store.getUrlNumEncountered(beta_url) == 2);
    assert(store.getUrlSeedDistance(beta_url) == 1);  // min(3, 1) = 1

    // Frontier should now have 4 total (3 + 1 new delta, beta was already known)
    total_frontier = 0;
    for (size_t i = 0; i < PRIORITY_BUCKETS; ++i) {
        std::lock_guard<std::mutex> lock(dc.buckets[i].bucket_lock);
        total_frontier += dc.buckets[i].urls.size();
    }
    assert(total_frontier == 4);

    cout << string("-> Passed test_batch_manage_frontier_and_update_url\n") << endl;
}

int main() {
    cout << string("Starting UrlStore Test Suite...\n") << endl;

    test_url_store_basic();

    // Order matters here: persistence must write the file before recover tries to read it
    test_url_store_persistence();
    test_url_store_recover();

    test_url_store_listener_test();
    test_manage_frontier_via_listener();
    test_manage_frontier_and_update_url();
    test_batch_manage_frontier_and_update_url();

    cout << string("All UrlStore tests passed successfully!") << endl;
    return 0;
}