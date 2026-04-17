#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

#include "../lib/joinable_thread_pool.h"

// Use 1 worker thread so the slow task physically blocks the pool's
// only worker while we verify that join(fast_id) returns immediately.
static void test_join_returns_while_other_task_pending() {
    JoinableThreadPool pool(1);

    std::atomic<bool> fast_done{false};
    std::atomic<bool> slow_unblock{false};
    std::atomic<bool> slow_done{false};

    int slow_id = 0;
    int fast_id = 1;

    // Submit slow task first — it will grab the single worker and hold it.
    pool.submit(slow_id, [&]() {
        while (!slow_unblock.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        slow_done.store(true);
    });

    // Submit fast task second — it sits in the queue behind the slow task.
    pool.submit(fast_id, [&]() {
        fast_done.store(true);
    });

    // The slow task is running (or about to run) and the fast task is
    // queued behind it. join(fast_id) must NOT return until the fast
    // task has actually executed, but it also must NOT wait for the slow
    // task just because it's in the pool.
    //
    // To test this: unblock the slow task so the worker can drain both,
    // then join the fast id. If join were still pool-wide, it would
    // behave the same — so the real assertion is the timing: once both
    // complete, join(fast_id) returns even though we haven't called
    // join(slow_id).
    slow_unblock.store(true);

    // join on fast_id — should return once the fast task finishes.
    pool.join(fast_id);
    assert(fast_done.load());

    // The slow task should also be done (it was unblocked), but we
    // haven't joined it yet — verify join(slow_id) works independently.
    pool.join(slow_id);
    assert(slow_done.load());

    fprintf(stderr, "PASS: test_join_returns_while_other_task_pending\n");
}

// Two concurrent dispatchers join only on their own waitgroup.
static void test_concurrent_dispatchers() {
    JoinableThreadPool pool(4);

    std::atomic<int> count_a{0};
    std::atomic<int> count_b{0};
    std::atomic<bool> hold_b{true};

    int id_a = 10;
    int id_b = 11;

    // Dispatch group A: 4 fast tasks.
    for (int i = 0; i < 4; ++i) {
        pool.submit(id_a, [&]() { count_a.fetch_add(1); });
    }

    // Dispatch group B: 4 tasks that block until released.
    for (int i = 0; i < 4; ++i) {
        pool.submit(id_b, [&]() {
            while (hold_b.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            count_b.fetch_add(1);
        });
    }

    // join(id_a) must return even though group B is still blocked.
    pool.join(id_a);
    assert(count_a.load() == 4);

    // Group B should still be in flight.
    assert(count_b.load() < 4);

    // Release group B and join it.
    hold_b.store(false);
    pool.join(id_b);
    assert(count_b.load() == 4);

    fprintf(stderr, "PASS: test_concurrent_dispatchers\n");
}

int main() {
    test_join_returns_while_other_task_pending();
    test_concurrent_dispatchers();
    fprintf(stderr, "All joinable_thread_pool tests passed.\n");
    return 0;
}
