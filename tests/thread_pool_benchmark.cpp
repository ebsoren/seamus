#include <iostream>
#include <vector>
#include <chrono>
#include <tuple>
#include "../thread_pool.h"

// A simple CPU-intensive function
long long fib(long long n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

void benchmark_thread_pool(size_t num_threads, int num_tasks, int fib_n) {
    std::cout << "--- Benchmarking with " << num_threads << " threads ---\n";

    auto start = std::chrono::high_resolution_clock::now();

    {
        ThreadPool<long long(*)(long long)> pool(fib, num_threads);
        for (int i = 0; i < num_tasks; ++i) {
            pool.enqueue_task(std::make_tuple(fib_n));
        }
    } // ThreadPool destructor is called here, which waits for all tasks to complete.

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Elapsed time: " << elapsed.count() << " ms\n";
}

int main() {
    const int num_tasks = 100; // Number of tasks to run
    const int fib_n = 35;      // Fibonacci number to calculate. Adjust if the benchmark is too light to be granular or too heavy.

    std::vector<size_t> thread_counts = {1, 2, 4, 6, 8, 10, 12, 14, 16};

    for (size_t count : thread_counts) {
        benchmark_thread_pool(count, num_tasks, fib_n);
    }

    return 0;
}
