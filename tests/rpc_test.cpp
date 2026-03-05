#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "../lib/vector.h"
#include "../lib/rpc_common.h"
#include "../lib/rpc_urlstore.h"
#include "../lib/rpc_listener.h"
#include "../lib/rpc_crawler.h"

static void print_header(const char* name) {
    printf("---- %s ----\n", name);
}

// Build a length-prefixed wire frame for `msg` and send it to localhost on `port`.
// Wire format matches recv_string: [4B big-endian length][payload].
static bool send_string_msg(uint16_t port, const char* msg) {
    size_t msg_len = strlen(msg);
    size_t total = sizeof(uint32_t) + msg_len;
    char* buf = new char[total];
    uint32_t net_len = htonl(static_cast<uint32_t>(msg_len));
    memcpy(buf, &net_len, sizeof(uint32_t));
    memcpy(buf + sizeof(uint32_t), msg, msg_len);
    string host("127.0.0.1");
    bool ok = send_buffer(host, port, buf, total);
    delete[] buf;
    return ok;
}

static bool vec_contains(const vector<string>& v, const char* s) {
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] == s) return true;
    }
    return false;
}

// Test: read a single message off one short-lived socket.
void test_single_message() {
    print_header("test_single_message");

    // Heap-allocate so the listener outlives the detached thread.
    // The thread holds a raw 'this' pointer inside listener_loop; if the
    // listener were on the stack, its memory would be reused by the next
    // test's frame while the thread is still spinning in accept().
    RPCListener* listener = new RPCListener(9100, 1);
    assert(listener->valid());

    std::mutex mtx;
    std::condition_variable cv;
    bool received = false;
    string result("");

    std::thread t([listener, &mtx, &cv, &received, &result]() {
        listener->listener_loop([&mtx, &cv, &received, &result](int fd) {
            auto msg = recv_string(fd);
            close(fd);
            if (msg) {
                std::lock_guard<std::mutex> lock(mtx);
                result = std::move(*msg);
                received = true;
                cv.notify_one();
            }
        });
    });
    t.detach();

    assert(send_string_msg(9100, "hello rpc"));

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return received; });

    assert(result == "hello rpc");
    printf("PASS\n");
}

// Test: read multiple messages each off their own short-lived socket.
void test_multiple_messages() {
    print_header("test_multiple_messages");

    RPCListener* listener = new RPCListener(9101, 4);
    assert(listener->valid());

    const char* messages[] = {"alpha", "bravo", "charlie", "delta", "echo"};
    const int N = 5;

    std::mutex mtx;
    std::condition_variable cv;
    vector<string> results;

    std::thread t([listener, &mtx, &cv, &results]() {
        listener->listener_loop([&mtx, &cv, &results](int fd) {
            auto msg = recv_string(fd);
            close(fd);
            if (msg) {
                std::lock_guard<std::mutex> lock(mtx);
                results.push_back(std::move(*msg));
                cv.notify_one();
            }
        });
    });
    t.detach();

    for (int i = 0; i < N; i++) {
        assert(send_string_msg(9101, messages[i]));
    }

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return static_cast<int>(results.size()) == N; });

    // Handler runs on thread pool so arrival order may vary; check set membership.
    for (int i = 0; i < N; i++) {
        assert(vec_contains(results, messages[i]));
    }

    printf("PASS\n");
}

// Test: serialize and deserialize a BatchURLStoreUpdateRequest over a real socket.
void test_batch_urlstore_roundtrip() {
    print_header("test_batch_urlstore_roundtrip");

    RPCListener* listener = new RPCListener(9102, 1);
    assert(listener->valid());

    std::mutex mtx;
    std::condition_variable cv;
    bool received = false;
    std::optional<BatchURLStoreUpdateRequest> result;

    std::thread t([listener, &mtx, &cv, &received, &result]() {
        listener->listener_loop([&mtx, &cv, &received, &result](int fd) {
            auto batch = recv_batch_urlstore_update(fd);
            close(fd);
            if (batch) {
                std::lock_guard<std::mutex> lock(mtx);
                result = std::move(batch);
                received = true;
                cv.notify_one();
            }
        });
    });
    t.detach();

    // Build a batch with two requests
    BatchURLStoreUpdateRequest batch;
    batch.reqs.push_back(URLStoreUpdateRequest{
        string("https://example.com"), string("example link"), 5, 2, 1
    });
    batch.reqs.push_back(URLStoreUpdateRequest{
        string("https://test.org/page"), string("test anchor"), 10, 3, 4
    });

    string host("127.0.0.1");
    assert(send_batch_urlstore_update(host, 9102, batch));

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return received; });

    assert(result.has_value());
    assert(result->reqs.size() == 2);

    // First request
    assert(result->reqs[0].url == "https://example.com");
    assert(result->reqs[0].anchor_text == "example link");
    assert(result->reqs[0].num_encountered == 5);
    assert(result->reqs[0].seed_list_url_hops == 2);
    assert(result->reqs[0].seed_list_domain_hops == 1);

    // Second request
    assert(result->reqs[1].url == "https://test.org/page");
    assert(result->reqs[1].anchor_text == "test anchor");
    assert(result->reqs[1].num_encountered == 10);
    assert(result->reqs[1].seed_list_url_hops == 3);
    assert(result->reqs[1].seed_list_domain_hops == 4);

    printf("PASS\n");
}

// Test: serialize and deserialize an empty batch.
void test_batch_urlstore_empty() {
    print_header("test_batch_urlstore_empty");

    RPCListener* listener = new RPCListener(9103, 1);
    assert(listener->valid());

    std::mutex mtx;
    std::condition_variable cv;
    bool received = false;
    std::optional<BatchURLStoreUpdateRequest> result;

    std::thread t([listener, &mtx, &cv, &received, &result]() {
        listener->listener_loop([&mtx, &cv, &received, &result](int fd) {
            auto batch = recv_batch_urlstore_update(fd);
            close(fd);
            if (batch) {
                std::lock_guard<std::mutex> lock(mtx);
                result = std::move(batch);
                received = true;
                cv.notify_one();
            }
        });
    });
    t.detach();

    BatchURLStoreUpdateRequest batch;
    string host("127.0.0.1");
    assert(send_batch_urlstore_update(host, 9103, batch));

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return received; });

    assert(result.has_value());
    assert(result->reqs.size() == 0);

    printf("PASS\n");
}

// Test: serialize and deserialize a BatchURLStoreDataRequest over a real socket.
void test_batch_urlstore_data_request_roundtrip() {
    print_header("test_batch_urlstore_data_request_roundtrip");

    RPCListener* listener = new RPCListener(9107, 1);
    assert(listener->valid());

    std::mutex mtx;
    std::condition_variable cv;
    bool received = false;
    std::optional<BatchURLStoreDataRequest> result;

    std::thread t([listener, &mtx, &cv, &received, &result]() {
        listener->listener_loop([&mtx, &cv, &received, &result](int fd) {
            auto batch = recv_batch_urlstore_data_request(fd);
            close(fd);
            if (batch) {
                std::lock_guard<std::mutex> lock(mtx);
                result = std::move(batch);
                received = true;
                cv.notify_one();
            }
        });
    });
    t.detach();

    BatchURLStoreDataRequest batch;
    batch.urls.push_back(string("https://example.com"));
    batch.urls.push_back(string("https://test.org/page"));
    batch.urls.push_back(string("https://foo.bar/baz?q=1"));

    string host("127.0.0.1");
    assert(send_batch_urlstore_data_request(host, 9107, batch));

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return received; });

    assert(result.has_value());
    assert(result->urls.size() == 3);
    assert(result->urls[0] == "https://example.com");
    assert(result->urls[1] == "https://test.org/page");
    assert(result->urls[2] == "https://foo.bar/baz?q=1");

    printf("PASS\n");
}

// Test: serialize and deserialize an empty BatchURLStoreDataRequest.
void test_batch_urlstore_data_request_empty() {
    print_header("test_batch_urlstore_data_request_empty");

    RPCListener* listener = new RPCListener(9108, 1);
    assert(listener->valid());

    std::mutex mtx;
    std::condition_variable cv;
    bool received = false;
    std::optional<BatchURLStoreDataRequest> result;

    std::thread t([listener, &mtx, &cv, &received, &result]() {
        listener->listener_loop([&mtx, &cv, &received, &result](int fd) {
            auto batch = recv_batch_urlstore_data_request(fd);
            close(fd);
            if (batch) {
                std::lock_guard<std::mutex> lock(mtx);
                result = std::move(batch);
                received = true;
                cv.notify_one();
            }
        });
    });
    t.detach();

    BatchURLStoreDataRequest batch;
    string host("127.0.0.1");
    assert(send_batch_urlstore_data_request(host, 9108, batch));

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return received; });

    assert(result.has_value());
    assert(result->urls.size() == 0);

    printf("PASS\n");
}

// Test: stop() causes listener_loop to exit and the thread becomes joinable.
void test_listener_stop() {
    print_header("test_listener_stop");

    RPCListener listener(9104, 1);
    assert(listener.valid());

    std::thread t([&listener]() {
        listener.listener_loop([](int fd) { close(fd); });
    });

    // Send a message to confirm the listener is running before we stop it
    assert(send_string_msg(9104, "ping"));

    listener.stop();
    t.join();

    // If we get here, the loop exited and the thread joined successfully
    printf("PASS\n");
}

// Test: stop() prevents accepting new connections.
void test_stop_rejects_connections() {
    print_header("test_stop_rejects_connections");

    RPCListener listener(9105, 1);
    assert(listener.valid());

    std::thread t([&listener]() {
        listener.listener_loop([](int fd) { close(fd); });
    });

    listener.stop();
    t.join();

    // After stop, sending should fail since the listen fd is closed
    bool ok = send_string_msg(9105, "should fail");
    assert(!ok);

    printf("PASS\n");
}

// Test: messages sent before stop() are still handled.
void test_stop_after_messages() {
    print_header("test_stop_after_messages");

    RPCListener listener(9106, 2);
    assert(listener.valid());

    std::mutex mtx;
    std::condition_variable cv;
    int count = 0;

    std::thread t([&listener, &mtx, &cv, &count]() {
        listener.listener_loop([&mtx, &cv, &count](int fd) {
            auto msg = recv_string(fd);
            close(fd);
            if (msg) {
                std::lock_guard<std::mutex> lock(mtx);
                count++;
                cv.notify_one();
            }
        });
    });

    // Send 3 messages, wait for all to be handled, then stop
    for (int i = 0; i < 3; i++) {
        assert(send_string_msg(9106, "msg"));
    }

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return count == 3; });
    }

    assert(count == 3);

    listener.stop();
    t.join();

    printf("PASS\n");
}

// Test: serialize and deserialize a BatchURLStoreDataResponse over a real socket.
void test_batch_urlstore_data_response_roundtrip() {
    print_header("test_batch_urlstore_data_response_roundtrip");

    RPCListener* listener = new RPCListener(9109, 1);
    assert(listener->valid());

    std::mutex mtx;
    std::condition_variable cv;
    bool received = false;
    std::optional<BatchURLStoreDataResponse> result;

    std::thread t([listener, &mtx, &cv, &received, &result]() {
        listener->listener_loop([&mtx, &cv, &received, &result](int fd) {
            auto batch = recv_batch_urlstore_data_response(fd);
            close(fd);
            if (batch) {
                std::lock_guard<std::mutex> lock(mtx);
                result = std::move(batch);
                received = true;
                cv.notify_one();
            }
        });
    });
    t.detach();

    BatchURLStoreDataResponse batch;
    UrlData d1{};
    d1.anchor_freqs.push_back(AnchorData{10, 3});
    d1.anchor_freqs.push_back(AnchorData{20, 7});
    d1.num_encountered = 42;
    d1.seed_distance = 2;
    d1.domain_dist = 1;
    d1.eot = 50;
    d1.eod = 120;

    UrlData d2{};
    d2.anchor_freqs.push_back(AnchorData{99, 1});
    d2.num_encountered = 5;
    d2.seed_distance = 0;
    d2.domain_dist = 0;
    d2.eot = 30;
    d2.eod = 80;

    batch.resps.push_back(std::move(d1));
    batch.resps.push_back(std::move(d2));

    string host("127.0.0.1");
    assert(send_batch_urlstore_data_response(host, 9109, batch));

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return received; });

    assert(result.has_value());
    assert(result->resps.size() == 2);

    // First UrlData
    assert(result->resps[0].anchor_freqs.size() == 2);
    assert(result->resps[0].anchor_freqs[0].anchor_id == 10);
    assert(result->resps[0].anchor_freqs[0].freq == 3);
    assert(result->resps[0].anchor_freqs[1].anchor_id == 20);
    assert(result->resps[0].anchor_freqs[1].freq == 7);
    assert(result->resps[0].num_encountered == 42);
    assert(result->resps[0].seed_distance == 2);
    assert(result->resps[0].domain_dist == 1);
    assert(result->resps[0].eot == 50);
    assert(result->resps[0].eod == 120);

    // Second UrlData
    assert(result->resps[1].anchor_freqs.size() == 1);
    assert(result->resps[1].anchor_freqs[0].anchor_id == 99);
    assert(result->resps[1].anchor_freqs[0].freq == 1);
    assert(result->resps[1].num_encountered == 5);
    assert(result->resps[1].seed_distance == 0);
    assert(result->resps[1].domain_dist == 0);
    assert(result->resps[1].eot == 30);
    assert(result->resps[1].eod == 80);

    printf("PASS\n");
}

// Test: serialize and deserialize an empty BatchURLStoreDataResponse.
void test_batch_urlstore_data_response_empty() {
    print_header("test_batch_urlstore_data_response_empty");

    RPCListener* listener = new RPCListener(9110, 1);
    assert(listener->valid());

    std::mutex mtx;
    std::condition_variable cv;
    bool received = false;
    std::optional<BatchURLStoreDataResponse> result;

    std::thread t([listener, &mtx, &cv, &received, &result]() {
        listener->listener_loop([&mtx, &cv, &received, &result](int fd) {
            auto batch = recv_batch_urlstore_data_response(fd);
            close(fd);
            if (batch) {
                std::lock_guard<std::mutex> lock(mtx);
                result = std::move(batch);
                received = true;
                cv.notify_one();
            }
        });
    });
    t.detach();

    BatchURLStoreDataResponse batch;
    string host("127.0.0.1");
    assert(send_batch_urlstore_data_response(host, 9110, batch));

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&] { return received; });

    assert(result.has_value());
    assert(result->resps.size() == 0);

    printf("PASS\n");
}

void test_crawl_target_serialize_deserialize_basic() {
    print_header("test_crawl_target_serialize_deserialize_basic");

    CrawlTarget original{string("example.com"), string("https://example.com/page"), 3, 1};

    size_t buf_size = crawl_target_serialized_size(original);
    char* buf = new char[buf_size];

    char* end = serialize_crawl_target(buf, original);
    assert(static_cast<size_t>(end - buf) == buf_size);

    CrawlTarget result{string(""), string(""), 0, 0};
    const char* read_end = deserialize_crawl_target(buf, buf_size, result);
    assert(read_end != nullptr);
    assert(static_cast<size_t>(read_end - buf) == buf_size);

    assert(result.domain == "example.com");
    assert(result.domain.size() == 11);
    assert(result.url == "https://example.com/page");
    assert(result.url.size() == 24);
    assert(result.seed_distance == 3);
    assert(result.domain_dist == 1);

    assert(original.domain == "example.com");
    assert(original.url == "https://example.com/page");
    assert(original.seed_distance == 3);
    assert(original.domain_dist == 1);

    delete[] buf;
}

void test_crawl_target_serialize_deserialize_empty_strings() {
    print_header("test_crawl_target_serialize_deserialize_empty_strings");

    CrawlTarget original{string(""), string(""), 0, 0};

    size_t buf_size = crawl_target_serialized_size(original);
    assert(buf_size == 12);

    char* buf = new char[buf_size];
    serialize_crawl_target(buf, original);

    CrawlTarget result{string(""), string(""), 0, 0};
    const char* read_end = deserialize_crawl_target(buf, buf_size, result);
    assert(read_end != nullptr);

    assert(result.domain == "");
    assert(result.domain.size() == 0);
    assert(result.url == "");
    assert(result.url.size() == 0);
    assert(result.seed_distance == 0);
    assert(result.domain_dist == 0);

    delete[] buf;
}

void test_crawl_target_serialize_deserialize_max_values() {
    print_header("test_crawl_target_serialize_deserialize_max_values");

    CrawlTarget original{string("test.org"), string("http://test.org"), UINT16_MAX, UINT16_MAX};

    size_t buf_size = crawl_target_serialized_size(original);
    char* buf = new char[buf_size];
    serialize_crawl_target(buf, original);

    CrawlTarget result{string(""), string(""), 0, 0};
    const char* read_end = deserialize_crawl_target(buf, buf_size, result);
    assert(read_end != nullptr);

    assert(result.domain == "test.org");
    assert(result.url == "http://test.org");
    assert(result.seed_distance == UINT16_MAX);
    assert(result.domain_dist == UINT16_MAX);

    delete[] buf;
}

void test_crawl_target_deserialize_truncated_buffer() {
    print_header("test_crawl_target_deserialize_truncated_buffer");

    CrawlTarget original{string("example.com"), string("https://example.com"), 1, 2};

    size_t buf_size = crawl_target_serialized_size(original);
    char* buf = new char[buf_size];
    serialize_crawl_target(buf, original);

    CrawlTarget result{string(""), string(""), 0, 0};

    assert(deserialize_crawl_target(buf, 2, result) == nullptr);
    assert(deserialize_crawl_target(buf, 6, result) == nullptr);
    assert(deserialize_crawl_target(buf, buf_size - 10, result) == nullptr);
    assert(deserialize_crawl_target(buf, buf_size - 1, result) == nullptr);

    delete[] buf;
}

void test_crawl_target_serialize_multiple_sequential() {
    print_header("test_crawl_target_serialize_multiple_sequential");

    CrawlTarget ct1{string("foo.com"), string("https://foo.com/a"), 1, 0};
    CrawlTarget ct2{string("bar.org"), string("https://bar.org/b"), 5, 3};

    size_t total = crawl_target_serialized_size(ct1) + crawl_target_serialized_size(ct2);
    char* buf = new char[total];

    char* mid = serialize_crawl_target(buf, ct1);
    char* end = serialize_crawl_target(mid, ct2);
    assert(static_cast<size_t>(end - buf) == total);

    CrawlTarget r1{string(""), string(""), 0, 0};
    CrawlTarget r2{string(""), string(""), 0, 0};
    const char* r_mid = deserialize_crawl_target(buf, total, r1);
    assert(r_mid != nullptr);
    size_t remaining = total - static_cast<size_t>(r_mid - buf);
    const char* r_end = deserialize_crawl_target(r_mid, remaining, r2);
    assert(r_end != nullptr);

    assert(r1.domain == "foo.com");
    assert(r1.url == "https://foo.com/a");
    assert(r1.seed_distance == 1);
    assert(r1.domain_dist == 0);

    assert(r2.domain == "bar.org");
    assert(r2.url == "https://bar.org/b");
    assert(r2.seed_distance == 5);
    assert(r2.domain_dist == 3);

    delete[] buf;
}

int main() {
    printf("\n===== RUNNING RPC TESTS =====\n\n");
    test_single_message();
    test_multiple_messages();
    test_batch_urlstore_roundtrip();
    test_batch_urlstore_empty();
    test_listener_stop();
    test_stop_rejects_connections();
    test_stop_after_messages();
    test_batch_urlstore_data_request_roundtrip();
    test_batch_urlstore_data_request_empty();
    test_batch_urlstore_data_response_roundtrip();
    test_batch_urlstore_data_response_empty();
    test_crawl_target_serialize_deserialize_basic();
    test_crawl_target_serialize_deserialize_empty_strings();
    test_crawl_target_serialize_deserialize_max_values();
    test_crawl_target_deserialize_truncated_buffer();
    test_crawl_target_serialize_multiple_sequential();
    printf("\n===== ALL RPC TESTS PASSED =====\n");
}
