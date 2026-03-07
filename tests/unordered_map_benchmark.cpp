//
// Created by Aiden Mizhen on 3/7/26.
//

#include <iostream>
#include <chrono>
#include <unordered_map>
#include "lib/unordered_map.h"

// Function to benchmark insertion (using operator[])
template <typename Map>
void benchmark_insert(Map& map, int num_operations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        map[i] = i;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "insert ([]): " << elapsed.count() << " ms\n";
}

// Function to benchmark successful lookups
template <typename Map>
void benchmark_find_success(Map& map, int num_operations) {
    auto start = std::chrono::high_resolution_clock::now();
    int found = 0; // Keeping a counter prevents the compiler from optimizing the loop away
    for (int i = 0; i < num_operations; ++i) {
        if (map.find(i) != map.end()) {
            found++;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "find (success): " << elapsed.count() << " ms (found " << found << ")\n";
}

// Function to benchmark failed lookups (tests probing worst-case)
template <typename Map>
void benchmark_find_fail(Map& map, int num_operations) {
    auto start = std::chrono::high_resolution_clock::now();
    int found = 0;
    // Search for keys we know we didn't insert
    for (int i = num_operations; i < num_operations * 2; ++i) {
        if (map.find(i) != map.end()) {
            found++;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "find (fail): " << elapsed.count() << " ms (found " << found << ")\n";
}

// Function to benchmark erase
template <typename Map>
void benchmark_erase(Map& map, int num_operations) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; ++i) {
        map.erase(i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "erase: " << elapsed.count() << " ms\n";
}

int main() {
    // 1 million operations to really stress test reallocation and cache locality
    const int num_operations = 1000000;

    std::cout << "--- Benchmarking custom unordered_map ---\n";
    unordered_map<int, int> my_map;
    benchmark_insert(my_map, num_operations);
    benchmark_find_success(my_map, num_operations);
    benchmark_find_fail(my_map, num_operations);
    benchmark_erase(my_map, num_operations);

    std::cout << "\n--- Benchmarking std::unordered_map ---\n";
    std::unordered_map<int, int> std_map;
    benchmark_insert(std_map, num_operations);
    benchmark_find_success(std_map, num_operations);
    benchmark_find_fail(std_map, num_operations);
    benchmark_erase(std_map, num_operations);

    return 0;
}