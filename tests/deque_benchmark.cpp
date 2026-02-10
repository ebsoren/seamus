#include <iostream>
#include <vector>
#include <chrono>
#include <deque>
#include "../deque.h"

// Function to benchmark push_back
template <typename Deque>
void benchmark_push_back(Deque& deque, int num_operations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        deque.push_back(i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "push_back: " << elapsed.count() << " ms\n";
}

// Function to benchmark push_front
template <typename Deque>
void benchmark_push_front(Deque& deque, int num_operations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        deque.push_front(i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "push_front: " << elapsed.count() << " ms\n";
}

// Function to benchmark pop_back
template <typename Deque>
void benchmark_pop_back(Deque& deque, int num_operations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        deque.pop_back();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "pop_back: " << elapsed.count() << " ms\n";
}

// Function to benchmark pop_front
template <typename Deque>
void benchmark_pop_front(Deque& deque, int num_operations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        deque.pop_front();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "pop_front: " << elapsed.count() << " ms\n";
}


int main() {
    const int num_operations = 100000;

    std::cout << "--- Benchmarking seamus::deque ---\n";
    deque<int> my_deque;
    benchmark_push_back(my_deque, num_operations);
    benchmark_push_front(my_deque, num_operations);
    benchmark_pop_back(my_deque, num_operations);
    benchmark_pop_front(my_deque, num_operations);

    std::cout << "\n--- Benchmarking std::deque ---\n";
    std::deque<int> std_deque;
    benchmark_push_back(std_deque, num_operations);
    benchmark_push_front(std_deque, num_operations);
    benchmark_pop_back(std_deque, num_operations);
    benchmark_pop_front(std_deque, num_operations);
    
    return 0;
}
