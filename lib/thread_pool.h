// Simple thread pool -- each class instance represents a pool of workers for the same func(args)

#include "vector.h"
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <queue>
#include <thread>


// Primary template (unused -- for compiler)
template <typename F>
class ThreadPool;


// Specified template -- ThreadPool instance applies to the same R(*)(Args...)
template <typename R, typename... Args>
class ThreadPool<R(*)(Args...)> {
public:

    // Typedefs
    using Func = R(*)(Args...);
    using TaskArgs = std::tuple<Args...>;


    // Construct thread pool with 'n' threads and set the target function for this instance
    ThreadPool(Func func, size_t n) : f(func), n_threads(n), exit(false) {
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
    }


    // Enqueue a task
    void enqueue_task(TaskArgs task) {
        {
            std::unique_lock<std::mutex> lock(m);
            tasks.push(task);
        }
        cv.notify_one();
    }


    // View the size of the task queue
    size_t task_queue_size() {
        m.lock();
        size_t res = tasks.size();
        m.unlock();

        return res;
    }


private:

    Func f;                         // The function every task in this pool runs
    std::mutex m;                   // Guard for task queue
    std::condition_variable cv;     // CV for threads
    size_t n_threads;               // Number of threads in the pool
    std::thread* threads;           // Underlying region of threads in the pool 
    std::queue<TaskArgs> tasks;     // Task queue
    bool exit;                      // Termination signal for worker threads


    // Loop for worker threads
    void worker_loop() {
        while (true) {
            TaskArgs task;

            // Grab the task in a locked scope, wait for exit signal or for tasks to not be empty
            {
                std::unique_lock<std::mutex> lock(m);
                cv.wait(lock, [&]{ return exit || !tasks.empty(); });

                if (exit && tasks.empty()) return;

                task = std::move(tasks.front());
                tasks.pop();
            }

            // Execute task outside lock
            std::apply([this](auto&&... args) { f(std::forward<decltype(args)>(args)...); }, task);
        }
    }
};
