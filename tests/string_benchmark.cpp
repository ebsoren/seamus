//
// Created by Aiden Mizhen on 2/15/26.
//

#include <iostream>
#include <chrono>
#include <string>
#include "../lib/string.h"

// 1. Industry-standard optimizer defeat (GCC/Clang)
// This forces the compiler to treat 'val' as an unknown side-effect,
// completely preventing Dead Store Elimination (DSE).
template <typename T>
inline void do_not_optimize(T const& val) {
#if defined(__clang__) || defined(__GNUC__)
    asm volatile("" : : "r,m"(val) : "memory");
#else
    // Fallback for MSVC
    volatile const char* p = reinterpret_cast<volatile const char*>(&val);
    (void)*p;
#endif
}

// Helper to encapsulate the warmup + timing logic
template <typename StringType>
void benchmark_sso_creation(int iterations, const std::string& label) {
    // --- WARMUP PHASE ---
    for (int i = 0; i < iterations / 10; ++i) {
        StringType s("short");
        do_not_optimize(s);
    }

    // --- TIMING PHASE ---
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        StringType s("short");
        do_not_optimize(s);
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << label << " SSO Creation (10 chars): " << elapsed.count() << " ms\n";
}

template <typename StringType>
void benchmark_heap_creation(int iterations, const std::string& label) {
    // --- WARMUP PHASE ---
    for (int i = 0; i < iterations / 10; ++i) {
        StringType s("this is a much longer string that definitely forces a heap allocation");
        do_not_optimize(s);
    }

    // --- TIMING PHASE ---
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        StringType s("this is a much longer string that definitely forces a heap allocation");
        do_not_optimize(s);
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << label << " Heap Creation (70 chars): " << elapsed.count() << " ms\n";
}

void benchmark_custom_join(int iterations) {
    string s1("pt1"), s2("pt2"), s3("pt3");

    // --- WARMUP PHASE ---
    for (int i = 0; i < iterations / 10; ++i) {
        string res = string::join("-", s1, s2, s3);
        do_not_optimize(res);
    }

    // --- TIMING PHASE ---
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        string res = string::join("-", s1, s2, s3);
        do_not_optimize(res);
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Variadic Join (Custom):     " << elapsed.count() << " ms\n";
}

void benchmark_std_concat(int iterations) {
    std::string s1("pt1"), s2("pt2"), s3("pt3");

    // --- WARMUP PHASE ---
    for (int i = 0; i < iterations / 10; ++i) {
        std::string res = s1 + "-" + s2 + "-" + s3;
        do_not_optimize(res);
    }

    // --- TIMING PHASE ---
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::string res = s1 + "-" + s2 + "-" + s3;
        do_not_optimize(res);
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Manual Concat (std):        " << elapsed.count() << " ms\n";
}

int main() {
    const int iterations = 10000000;

    std::cout << "\n--- Benchmarking std::string ---\n";
    benchmark_sso_creation<std::string>(iterations, "[std]  ");
    benchmark_heap_creation<std::string>(iterations, "[std]  ");
    benchmark_std_concat(iterations);

    std::cout << "--- Benchmarking Aiden::string ---\n";
    benchmark_sso_creation<string>(iterations, "[Aiden]");
    benchmark_heap_creation<string>(iterations, "[Aiden]");
    benchmark_custom_join(iterations);

    return 0;
}