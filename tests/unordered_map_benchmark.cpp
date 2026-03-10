#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <functional> // Required for the isolation test

// Replace with your actual headers
#include "lib/string.h"
#include "lib/unordered_map.h"
// #include "lib/vector.h" // Uncomment when your custom vector is ready

using namespace std::chrono;

// Simple Timer Utility
class Timer {
    high_resolution_clock::time_point start;
public:
    Timer() : start(high_resolution_clock::now()) {}
    double elapsed_ms() {
        auto end = high_resolution_clock::now();
        return duration_cast<duration<double, std::milli>>(end - start).count();
    }
};

// ==========================================
// DATA GENERATION (Mimicking Search Engines)
// ==========================================

std::vector<std::string> generate_search_corpus(size_t num_tokens) {
    std::vector<std::string> corpus;
    corpus.reserve(num_tokens);

    std::vector<std::string> vocabulary = {
        "the", "and", "of", "to", "a", "in", "that", "is", "was", "he", // High freq (short strings)
        "artificial", "intelligence", "algorithm", "optimization",      // Medium freq (long strings)
        "supercalifragilisticexpialidocious", "uncharacteristically"    // Low freq
    };

    std::mt19937 gen(42);
    std::exponential_distribution<> d(1.5);

    for (size_t i = 0; i < num_tokens; ++i) {
        int index = static_cast<int>(d(gen)) % vocabulary.size();
        if (i % 100 == 0) {
            corpus.push_back("unique_token_" + std::to_string(i));
        } else {
            corpus.push_back(vocabulary[index]);
        }
    }
    return corpus;
}

// ==========================================
// BENCHMARKS
// ==========================================

void BM_Ingestion_STL(const std::vector<std::string>& corpus) {
    std::unordered_map<std::string, int> stl_map;
    stl_map.reserve(corpus.size() / 4);

    Timer t;
    for (const auto& word : corpus) {
        stl_map[word]++;
    }
    std::cout << "STL Ingestion Time:     " << t.elapsed_ms() << " ms\n";
}

void BM_Ingestion_Custom(const std::vector<std::string>& corpus) {
    unordered_map<string, int> custom_map;
    // reserve() safely calculates the next power of 2!
    custom_map.reserve(corpus.size() / 4);

    Timer t;
    for (const auto& word : corpus) {
        string_view sv(word.c_str(), word.length());
        custom_map[sv]++;
    }
    std::cout << "Custom Ingestion Time:  " << t.elapsed_ms() << " ms\n";
}

void BM_ZeroCopy_vs_Copy_Lookup(const std::vector<std::string>& corpus) {
    std::cout << "\n--- Zero-Copy vs Allocation Lookup Benchmark ---\n";
    unordered_map<string, int> map;
    map.reserve(corpus.size());

    for (const auto& word : corpus) {
        map[string_view(word.c_str(), word.length())] = 1;
    }

    Timer t1;
    size_t hits_alloc = 0;
    for (const auto& word : corpus) {
        string search_key(word.c_str(), word.length());
        if (map.get(search_key) != nullptr) {
            hits_alloc++;
        }
    }
    std::cout << "Lookup WITH allocation: " << t1.elapsed_ms() << " ms\n";

    Timer t2;
    size_t hits_zero_copy = 0;
    for (const auto& word : corpus) {
        string_view search_key(word.c_str(), word.length());
        if (map.get(search_key) != nullptr) {
            hits_zero_copy++;
        }
    }
    std::cout << "Lookup ZERO-COPY (sv):  " << t2.elapsed_ms() << " ms\n";

    if (hits_alloc != hits_zero_copy) std::cout << "Mismatch error!\n";
}

void BM_Search_Engine_Query_Simulation() {
    std::cout << "\n--- Search Engine Query Simulation ---\n";

    // SAFELY INITIALIZING WITH RESERVE
    unordered_map<string, int> custom_map;
    custom_map.reserve(10000); // Will round up to 16384

    std::unordered_map<std::string, int> stl_map;
    stl_map.reserve(10000);

    for(int i = 0; i < 10000; i++) {
        std::string s = "doc_term_" + std::to_string(i);
        custom_map.insert(string(s.c_str(), s.length()), i);
        stl_map.insert({s, i});
    }

    const int num_queries = 5000000;

    Timer t_stl;
    size_t stl_sum = 0;
    for(int i = 0; i < num_queries; i++) {
        std::string query = "doc_term_" + std::to_string(i % 20000);
        auto it = stl_map.find(query);
        if (it != stl_map.end()) stl_sum += it->second;
    }
    std::cout << "STL 5M Queries:         " << t_stl.elapsed_ms() << " ms\n";

    Timer t_custom;
    size_t custom_sum = 0;
    for(int i = 0; i < num_queries; i++) {
        std::string query = "doc_term_" + std::to_string(i % 20000);
        string_view sv(query.c_str(), query.length());

        const int* val = custom_map.get(sv);
        if (val != nullptr) custom_sum += *val;
    }
    std::cout << "Custom 5M Queries:      " << t_custom.elapsed_ms() << " ms\n";

    if (stl_sum != custom_sum) std::cout << "Sum mismatch!\n";
}

void BM_Container_Isolation_Test(const std::vector<std::string>& corpus) {
    std::cout << "\n--- Container Isolation Test (Pure Architecture Comparison) ---\n";
    std::cout << "(Both containers using std::string, std::hash, and standard copies)\n";

    std::unordered_map<std::string, int> stl_map;
    stl_map.reserve(corpus.size() / 4);

    // Instantiate custom map using standard STL types to level the playing field
    unordered_map<std::string, int, std::hash<std::string>, std::equal_to<std::string>> custom_map;
    custom_map.reserve(corpus.size() / 4); // Rounds up safely

    Timer t_stl_ingest;
    for (const auto& word : corpus) {
        stl_map[word]++;
    }
    std::cout << "STL Map Ingestion:      " << t_stl_ingest.elapsed_ms() << " ms\n";

    Timer t_custom_ingest;
    for (const auto& word : corpus) {
        custom_map[word]++;
    }
    std::cout << "Custom Map Ingestion:   " << t_custom_ingest.elapsed_ms() << " ms\n";

    size_t stl_hits = 0;
    Timer t_stl_query;
    for (const auto& word : corpus) {
        auto it = stl_map.find(word);
        if (it != stl_map.end()) stl_hits += it->second;
    }
    std::cout << "STL Map Queries:        " << t_stl_query.elapsed_ms() << " ms\n";

    size_t custom_hits = 0;
    Timer t_custom_query;
    for (const auto& word : corpus) {
        const int* val = custom_map.get(word);
        if (val != nullptr) custom_hits += *val;
    }
    std::cout << "Custom Map Queries:     " << t_custom_query.elapsed_ms() << " ms\n";

    if (stl_hits != custom_hits) std::cout << "Mismatch error in isolation test!\n";
}

int main() {
    std::cout << "Generating 1 Million token search corpus...\n\n";
    const size_t NUM_TOKENS = 1000000;
    auto corpus = generate_search_corpus(NUM_TOKENS);

    std::cout << "--- Ingestion Benchmark (1 Million Tokens) ---\n";
    BM_Ingestion_STL(corpus);
    BM_Ingestion_Custom(corpus);

    BM_ZeroCopy_vs_Copy_Lookup(corpus);

    BM_Search_Engine_Query_Simulation();

    BM_Container_Isolation_Test(corpus);

    return 0;
}