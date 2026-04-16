// Joinable thread pool — submit a batch of tasks, then block in join()
// until every submitted task has finished. The pool stays alive across
// batches (reusable). Intended for fan-out / fan-in patterns where the
// caller dispatches N independent tasks and then needs all of them to
// have completed before proceeding.
//
// Concurrency note: join() waits until the pool-wide pending count
// reaches zero — i.e. every currently submitted task has finished. If
// two threads submit their fan-outs into the same pool concurrently,
// each thread's join() will wait for BOTH batches. That's fine for a
// single-threaded dispatcher (e.g. a synchronous RPC handler) but not
// for per-caller batch semantics — use a countdown latch scoped to the
// caller's batch in that case.
#pragma once

#include "deque.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>


class JoinableThreadPool {
public:
    explicit JoinableThreadPool(size_t n)
        : n_threads(n), pending(0), exit_flag(false) {
        threads = new std::thread[n_threads];
        for (size_t i = 0; i < n_threads; ++i) {
            threads[i] = std::thread(&JoinableThreadPool::worker_loop, this);
        }
    }

    ~JoinableThreadPool() {
        {
            std::lock_guard<std::mutex> lk(m);
            exit_flag = true;
        }
        worker_cv.notify_all();
        for (size_t i = 0; i < n_threads; ++i) {
            if (threads[i].joinable()) threads[i].join();
        }
        delete[] threads;
    }

    JoinableThreadPool(const JoinableThreadPool &) = delete;
    JoinableThreadPool &operator=(const JoinableThreadPool &) = delete;

    // Submit a task. Increments the pool-wide pending count and wakes
    // one worker.
    void submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lk(m);
            tasks.push_back(std::move(task));
            ++pending;
        }
        worker_cv.notify_one();
    }

    // Block until every submitted task has run to completion and the
    // pool is idle (pending == 0). Reusable across batches.
    void join() {
        std::unique_lock<std::mutex> lk(m);
        join_cv.wait(lk, [this]{ return pending == 0; });
    }

    size_t thread_count() const { return n_threads; }

private:
    std::mutex m;
    std::condition_variable worker_cv;       // workers wait for tasks
    std::condition_variable join_cv;         // joiners wait for idle
    size_t n_threads;
    std::thread *threads;
    deque<std::function<void()>> tasks;
    size_t pending;                          // submitted but not finished
    bool exit_flag;

    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lk(m);
                worker_cv.wait(lk, [&]{ return exit_flag || !tasks.empty(); });
                if (exit_flag && tasks.empty()) return;
                task = std::move(tasks.front());
                tasks.pop_front();
            }

            // Task exceptions would otherwise unwind out of the worker
            // and skip the pending decrement — wedging join(). Swallow
            // them here so the bookkeeping always runs.
            try {
                task();
            } catch (...) {
                // intentionally ignored
            }

            bool idle = false;
            {
                std::lock_guard<std::mutex> lk(m);
                --pending;
                if (pending == 0) idle = true;
            }
            if (idle) join_cv.notify_all();
        }
    }
};
