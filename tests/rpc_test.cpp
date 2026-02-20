#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "../lib/vector.h"
#include "../lib/rpc_common.h"
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

int main() {
    printf("\n===== RUNNING RPC TESTS =====\n\n");
    test_single_message();
    test_multiple_messages();
    printf("\n===== ALL RPC TESTS PASSED =====\n");
}
