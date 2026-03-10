#include <cassert>
#include <iostream>
#include <utility> // for std::move, purely for testing

// Replace with your actual headers
#include "lib/string.h"
#include "lib/vector.h"
#include "lib/unordered_map.h"

void test_empty_map() {
    unordered_map<int, string> map;
    assert(map.size() == 0);
    assert(map.find(1) == map.end());
}

void test_single_insert_and_find() {
    unordered_map<int, string> map;
    // Using insert with rvalues because operator[] requires a default constructor
    map.insert(1, string("apple"));

    assert(map.size() == 1);
    auto it = map.find(1);
    assert(it != map.end());
    assert((*it).value == "apple");
    assert((*it).key == 1);
}

void test_insert_duplicate_key() {
    unordered_map<int, string> map;
    map.insert(1, string("apple"));
    map.insert(1, string("banana")); // Should overwrite

    assert(map.size() == 1);
    auto it = map.find(1);
    assert((*it).value == "banana");
}

void test_operator_bracket() {
    // We test [] using an int value, since int has a default constructor (0)
    unordered_map<int, int> map;

    // Test insertion via []
    map[10] = 50;
    assert(map.size() == 1);
    assert(map[10] == 50);

    // Test updating via []
    map[10] = 99;
    assert(map.size() == 1);
    assert(map[10] == 99);
}

void test_find_not_present() {
    unordered_map<int, string> map;
    map.insert(1, string("apple"));

    assert(map.find(2) == map.end());
}

void test_rehashing_and_large_inserts() {
    unordered_map<int, int> map(8); // Start with a tiny capacity

    const int num_elements = 1000;
    for (int i = 0; i < num_elements; ++i) {
        map.insert(i, i * 10);
    }

    assert(map.size() == num_elements);
    assert(map.capacity() >= num_elements);

    for (int i = 0; i < num_elements; ++i) {
        auto it = map.find(i);
        assert(it != map.end());
        assert((*it).value == i * 10);
    }
}

void test_erase_existing_element() {
    unordered_map<int, string> map;
    map.insert(1, string("apple"));
    map.insert(2, string("banana"));

    bool erased = map.erase(1);
    assert(erased == true);
    assert(map.size() == 1);
    assert(map.find(1) == map.end());
    assert(map.find(2) != map.end());
}

void test_erase_non_existing_element() {
    unordered_map<int, string> map;
    map.insert(1, string("apple"));

    bool erased = map.erase(99);
    assert(erased == false);
    assert(map.size() == 1);
}

void test_tombstone_recycling() {
    unordered_map<int, string> map;

    map.insert(1, string("apple"));
    map.erase(1);
    assert(map.size() == 0);

    map.insert(2, string("banana"));
    assert(map.size() == 1);
    assert(map.find(2) != map.end());
}

void test_iterators() {
    unordered_map<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);

    int count = 0;
    int sum_keys = 0;

    for (auto it = map.begin(); it != map.end(); ++it) {
        count++;
        sum_keys += (*it).key;
    }

    assert(count == 3);
    assert(sum_keys == 6);
}

void test_complex_types_memory_safety() {
    // Vector DOES have a default constructor, so [] works here!
    unordered_map<int, vector<string>> map;

    vector<string> vec1;
    vec1.push_back(string("hello"));
    vec1.push_back(string("world"));
    map.insert(1, std::move(vec1));

    vector<string> vec2;
    vec2.push_back(string("c++"));
    vec2.push_back(string("hash"));
    map.insert(2, std::move(vec2));

    map.erase(1);

    map[2].push_back(string("rock"));

    assert(map.size() == 1);
    assert(map[2].size() == 3);
}

void test_reserve() {
    unordered_map<int, int> map;
    map.reserve(500);

    assert(map.capacity() >= 512);

    map.insert(1, 100);
    auto it = map.find(1);
    assert((*it).value == 100);
}

void test_probe_chain_tombstone_traversal() {
    unordered_map<int, string> map(8); // Small map to force collisions easily

    // We need to force a collision. Assuming DefaultHash for int just casts to size_t.
    // In a map of capacity 8, keys 1 and 9 will both hash to index 1 (1 % 8 == 1, 9 % 8 == 1).
    map.insert(1, string("first"));
    map.insert(9, string("collided"));

    assert(map.size() == 2);

    // Erase the first one. This leaves a DELETED state at index 1.
    map.erase(1);

    // The critical test: Can we still find the collided element?
    // If find_index stops at DELETED, this will fail. It must continue to index 2.
    auto it = map.find(9);
    assert(it != map.end());
    assert((*it).value == "collided");
}

void test_const_get_method() {
    unordered_map<string, int> map;
    map.insert(string("readonly"), 100);

    // Create a const reference to the map
    const auto& cmap = map;

    // operator[] will not compile on a const map, so we must use get() or find()
    const int* val = cmap.get(string_view("readonly", 8));
    assert(val != nullptr);
    assert(*val == 100);

    const int* missing = cmap.get(string_view("ghost", 5));
    assert(missing == nullptr);
}

void test_rvalue_operator_bracket() {
    unordered_map<string, int> map;

    string my_key("moved_key");
    // Use your custom ::move to force the rvalue overload
    map[::move(my_key)] = 500;

    assert(map.size() == 1);

    string_view sv("moved_key", 9);
    assert(map[sv] == 500);
}

// ==========================================
// SEARCH ENGINE ARCHITECTURE TESTS
// ==========================================

void test_string_view_lookup_existing() {
    // Here string is the KEY, int is the VALUE.
    // Int has a default constructor, so we can test []!
    unordered_map<string, int> map;

    map.insert(string("document"), 42);

    const char* buffer = "document";
    string_view sv(buffer, 8);

    assert(map.size() == 1);
    assert(map[sv] == 42);

    auto it = map.find(sv);
    assert(it != map.end());
    assert((*it).value == 42);
}

void test_string_view_lookup_non_existing() {
    unordered_map<string, int> map;

    const char* buffer = "indexing";
    string_view sv(buffer, 8);

    // Lazy allocation works here because Value (int) can be default constructed
    map[sv] = 100;

    assert(map.size() == 1);

    // We can't use map[string("indexing")] because string has no default constructor
    // but we can look it up with find() to verify it exists!
    auto it = map.find(string("indexing"));
    assert(it != map.end());
    assert((*it).value == 100);
}

void test_sso_boundary() {
    unordered_map<string, int> map;

    string short_str("short_word");
    string long_str("this_is_a_very_long_word");

    map.insert(std::move(short_str), 1);
    map.insert(std::move(long_str), 2);

    assert(map.size() == 2);

    string_view short_sv("short_word", 10);
    string_view long_sv("this_is_a_very_long_word", 24);

    assert(map[short_sv] == 1);
    assert(map[long_sv] == 2);
}

int main() {
    test_empty_map();
    test_single_insert_and_find();
    test_insert_duplicate_key();
    test_operator_bracket();
    test_find_not_present();
    test_rehashing_and_large_inserts();
    test_erase_existing_element();
    test_erase_non_existing_element();
    test_tombstone_recycling();
    test_iterators();
    test_complex_types_memory_safety();
    test_reserve();
    test_string_view_lookup_existing();
    test_string_view_lookup_non_existing();
    test_sso_boundary();
    test_probe_chain_tombstone_traversal();
    test_const_get_method();
    test_rvalue_operator_bracket();

    std::cout << "All strict unordered_map tests passed!\n";
    return 0;
}