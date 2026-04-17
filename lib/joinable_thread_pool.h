// Joinable thread pool with named waitgroups — submit tasks under an
// integer task ID, then join() on that specific ID. Multiple callers
// can fan out into the same pool concurrently without blocking on each
// other's work.
//
// Usage:
//   pool.submit(id, fn);   // increment waitgroup 'id', enqueue fn
//   pool.join(id);          // block until waitgroup 'id' reaches zero
//
// Waitgroup entries are created on first submit and cleaned up on join
// once the count reaches zero.
#pragma once

#include "deque.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>


class JoinableThreadPool {
public:
    explicit JoinableThreadPool(size_t n)
        : n_threads(n), exit_flag(false) {
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

    // Submit a task under the given waitgroup ID.
    void submit(int id, std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lk(m);
            tasks.push_back({id, std::move(task)});
            ++waitgroups[id];
        }
        worker_cv.notify_one();
    }

    // Block until every task submitted under 'id' has completed.
    void join(int id) {
        std::unique_lock<std::mutex> lk(m);
        join_cv.wait(lk, [&]{ return waitgroups.find(id) == waitgroups.end() || waitgroups[id] == 0; });
        waitgroups.erase(id);
    }

    size_t thread_count() const { return n_threads; }

private:
    struct TaggedTask {
        int id;
        std::function<void()> fn;
    };

    std::mutex m;
    std::condition_variable worker_cv;       // workers wait for tasks
    std::condition_variable join_cv;         // joiners wait for their group
    size_t n_threads;
    std::thread *threads;
    deque<TaggedTask> tasks;
    std::unordered_map<int, size_t> waitgroups;  // id -> pending count
    bool exit_flag;

    void worker_loop() {
        while (true) {
            TaggedTask tt;
            {
                std::unique_lock<std::mutex> lk(m);
                worker_cv.wait(lk, [&]{ return exit_flag || !tasks.empty(); });
                if (exit_flag && tasks.empty()) return;
                tt = std::move(tasks.front());
                tasks.pop_front();
            }

            try {
                tt.fn();
            } catch (...) {
                // intentionally ignored
            }

            {
                std::lock_guard<std::mutex> lk(m);
                auto it = waitgroups.find(tt.id);
                if (it != waitgroups.end()) {
                    --it->second;
                }
            }
            join_cv.notify_all();
        }
    }
};
