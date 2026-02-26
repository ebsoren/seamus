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

int main() {
    printf("\n===== RUNNING RPC TESTS =====\n\n");
    test_single_message();
    test_multiple_messages();
    test_batch_urlstore_roundtrip();
    test_batch_urlstore_empty();
    test_listener_stop();
    test_stop_rejects_connections();
    test_stop_after_messages();
    printf("\n===== ALL RPC TESTS PASSED =====\n");
}
