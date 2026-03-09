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
    string fileName = string::join("urlstore_", string(worker_number), ".txt");
    remove(fileName.data());
}

// Tests getters, setters for class data structures as well as content title and body detection
void test_url_store_basic() {
    cout << string("Running test_url_store_basic...") << endl;
    UrlStore store;
    
    // Test 1: Adding a new URL
    vector<string> anchors1;
    anchors1.push_back(string("michigan"));
    anchors1.push_back(string("home"));
    
    bool added = store.addUrl(string("https://umich.edu"), anchors1, 2, 1, 5, 15, 1);
    assert(added == true);
    
    // Verify basic getters
    assert(store.getUrlNumEncountered(string("https://umich.edu")) == 1);
    assert(store.getUrlSeedDistance(string("https://umich.edu")) == 2);
    
    // Verify text positioning
    assert(store.inTitle(string("https://umich.edu"), 3) == true);   // 3 < eot (5)
    assert(store.inTitle(string("https://umich.edu"), 6) == false);  // 6 > eot (5)
    assert(store.inDescription(string("https://umich.edu"), 10) == true); // 5 <= 10 < eod (15)
    assert(store.inDescription(string("https://umich.edu"), 20) == false); // 20 > eod (15)

    // Test 2: Updating an existing URL
    vector<string> anchors2;
    anchors2.push_back(string("michigan")); // duplicate to increase freq
    anchors2.push_back(string("university")); // new anchor
    
    bool updated = store.updateUrl(string("https://umich.edu"), anchors2, 1, 1, 3);
    assert(updated == true);
    
    // Verify counters and minimum distances updated properly
    assert(store.getUrlNumEncountered(string("https://umich.edu")) == 4); // 1 + 3
    assert(store.getUrlSeedDistance(string("https://umich.edu")) == 1);   // min(2, 1)

    // Verify Anchor Data
    auto anchor_info = store.getUrlAnchorInfo(string("https://umich.edu"));
    assert(anchor_info.size() == 3); // "michigan", "home", "university"
    
    // Ensure duplicate anchors added correctly (michigan should have freq 2)
    for (const auto& a : anchor_info) {
        if (*(a.anchor_text) == string("michigan")) {
            assert(a.freq == 2);
        } else {
            assert(a.freq == 1);
        }
    }
    
    // Test 3: Updating a non-existent URL should fail
    bool failed_update = store.updateUrl(string("https://doesnotexist.com"), anchors1, 1, 1, 1);
    assert(failed_update == false);

    cout << string("-> Passed test_url_store_basic\n") << endl;
}

void test_url_store_persistence() {
    cout << string("Running test_url_store_persistence...") << endl;
    cleanup_test_file(WORKER_NUMBER);

    UrlStore store;
    vector<string> anchors;
    anchors.push_back(string("persisted link"));
    
    store.addUrl(string("http://persist.me"), anchors, 5, 2, 10, 20, 42);
    
    // This should write to urlstore_0_tmp.txt and rename to urlstore_0.txt
    store.persist();
    
    // Basic sanity check that the file can be opened
    string fileName = string::join("urlstore_", string(WORKER_NUMBER), ".txt");
    FILE* fd = fopen(fileName.data(), string("r").data());
    assert(fd != nullptr);
    fclose(fd);

    cout << string("-> Passed test_url_store_persistence\n") << endl;
}

void test_url_store_recover() {
    cout << string("Running test_url_store_recover...") << endl;
    // Note: Depends on test_url_store_persistence running first!
    
    UrlStore store; // Fresh instance, empty in memory
    
    // Read from the file we just persisted
    UrlStore::readFromFile(store, WORKER_NUMBER);
    
    // Validate the parsed data matches exactly what we injected in the previous test
    assert(store.getUrlNumEncountered(string("http://persist.me")) == 42);
    assert(store.getUrlSeedDistance(string("http://persist.me")) == 5);
    assert(store.inTitle(string("http://persist.me"), 5) == true);
    assert(store.inDescription(string("http://persist.me"), 15) == true);
    
    auto anchor_info = store.getUrlAnchorInfo(string("http://persist.me"));
    assert(anchor_info.size() == 1);
    assert(*(anchor_info[0].anchor_text) == string("persisted link"));
    assert(anchor_info[0].freq == 1);

    cleanup_test_file(WORKER_NUMBER); // Clean up after ourselves
    cout << string("-> Passed test_url_store_recover\n") << endl;
}

void test_url_store_listener_test() {
    cout << string("Running test_url_store_listener_test...") << endl;
    
    // Spinning up this object will automatically start the background RPCListener thread on PORT 9000
    UrlStore store; 
    
    // Build a mock network request simulating a worker finding a new URL
    BatchURLStoreUpdateRequest batch;
    URLStoreUpdateRequest req;
    req.url = string("http://rpc-test.com");
    req.anchor_text = string("rpc anchor");
    req.num_encountered = 1;
    req.seed_list_url_hops = 3;
    req.seed_list_domain_hops = 3;
    batch.reqs.push_back(req);

    // Send the batch to our own listener
    bool send_success = send_batch_urlstore_update(string("127.0.0.1"), PORT, batch);
    assert(send_success == true);

    // Give the listener thread a brief moment to accept the socket, read, and mutate state
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // Verify the background thread correctly invoked client_handler and mutated our UrlStore
    assert(store.getUrlNumEncountered(string("http://rpc-test.com")) == 1);
    assert(store.getUrlSeedDistance(string("http://rpc-test.com")) == 3);
    
    auto anchor_info = store.getUrlAnchorInfo(string("http://rpc-test.com"));
    assert(anchor_info.size() == 1);
    assert(*(anchor_info[0].anchor_text) == string("rpc anchor"));

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