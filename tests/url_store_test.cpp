#include "../url_store/url_store.h"
#include "../lib/rpc_urlstore.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

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
    UrlStore store;
    
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
    bool failed_update = store.updateUrl(dne_url, anchors1, 1, 1, 1);
    assert(failed_update == false);

    cout << string("-> Passed test_url_store_basic\n") << endl;
}

void test_url_store_persistence() {
    cout << string("Running test_url_store_persistence...") << endl;
    cleanup_test_file(URL_STORE_WORKER_NUMBER);

    UrlStore store;
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
    UrlStore store; // Fresh instance, empty in memory
    
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
    UrlStore store; 
    
    // Build a mock network request simulating a worker finding a new URL
    BatchURLStoreUpdateRequest batch;
    URLStoreUpdateRequest req;
    
    // Keep these as rvalues so the struct can invoke move-assignment
    req.url = string("http://rpc-test.com");
    req.anchor_text = string("rpc anchor");
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

int main() {
    cout << string("Starting UrlStore Test Suite...\n") << endl;

    test_url_store_basic();
    
    // Order matters here: persistence must write the file before recover tries to read it
    test_url_store_persistence();
    test_url_store_recover();
    
    test_url_store_listener_test();

    cout << string("All UrlStore tests passed successfully!") << endl;
    return 0;
}