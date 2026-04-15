// Simple thread pool -- workers pull and execute std::function<void()> tasks from a shared queue
#pragma once

#include "deque.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>


class ThreadPool {
public:

    // Construct thread pool with 'n' worker threads
    ThreadPool(size_t n) : n_threads(n), exit(false) {
        threads = new std::thread[n_threads];

        for (size_t i = 0; i < n_threads; ++i) {
            threads[i] = std::thread(&ThreadPool::worker_loop, this);
        }
    }


    // Destructor -- send termination signal to all threads and join
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(m);
            exit = true;
        }
        cv.notify_all();

        for (size_t i = 0; i < n_threads; ++i) {
            if (threads[i].joinable()) threads[i].join();
        }

        delete[] threads;
    }


    // Enqueue a task -- caller binds any arguments into the callable before passing
    void enqueue_task(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(m);
            tasks.push_back(std::move(task));
        }
        cv.notify_one();
    }


    // View the size of the task queue
    size_t task_queue_size() {
        std::lock_guard<std::mutex> lock(m);
        return tasks.size();
    }


private:

    std::mutex m;                           // Guard for task queue
    std::condition_variable cv;             // CV for worker threads
    size_t n_threads;                       // Number of threads in the pool
    std::thread* threads;                   // Underlying region of threads in the pool
    deque<std::function<void()>> tasks;     // Task queue
    bool exit;                              // Termination signal for worker threads


    // Loop for worker threads
    void worker_loop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(m);
                cv.wait(lock, [&]{ return exit || !tasks.empty(); });

                if (exit && tasks.empty()) return;

                task = std::move(tasks.front());
                tasks.pop_front();
            }

            // Execute task outside lock
            task();
        }
    }
};
