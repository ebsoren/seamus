#include <cassert>
#include <string>
#include <iostream>
#include <vector>
#include "lib/unordered_map.h" // Replace with your actual header name

void test_empty_map() {
    unordered_map<int, std::string> map;
    assert(map.size() == 0);
    assert(map.find(1) == map.end());
}

void test_single_insert_and_find() {
    unordered_map<int, std::string> map;
    map.insert(1, "apple");

    assert(map.size() == 1);
    auto it = map.find(1);
    assert(it != map.end());
    assert((*it).value == "apple");
    assert((*it).key == 1);
}

void test_insert_duplicate_key() {
    unordered_map<int, std::string> map;
    map.insert(1, "apple");
    map.insert(1, "banana"); // Should overwrite

    assert(map.size() == 1);
    assert(map[1] == "banana");
}

void test_operator_bracket() {
    unordered_map<int, std::string> map;

    // Test insertion via []
    map[10] = "cherry";
    assert(map.size() == 1);
    assert(map[10] == "cherry");

    // Test updating via []
    map[10] = "date";
    assert(map.size() == 1);
    assert(map[10] == "date");
}

void test_find_not_present() {
    unordered_map<int, std::string> map;
    map.insert(1, "apple");

    assert(map.find(2) == map.end());
}

void test_rehashing_and_large_inserts() {
    unordered_map<int, int> map(8); // Start with a tiny capacity

    // Insert enough elements to force multiple reallocations
    const int num_elements = 1000;
    for (int i = 0; i < num_elements; ++i) {
        map.insert(i, i * 10);
    }

    assert(map.size() == num_elements);
    assert(map.capacity() >= num_elements); // Capacity should have grown

    // Verify all elements survived the rehashes
    for (int i = 0; i < num_elements; ++i) {
        auto it = map.find(i);
        assert(it != map.end());
        assert((*it).value == i * 10);
    }
}

void test_erase_existing_element() {
    unordered_map<int, std::string> map;
    map.insert(1, "apple");
    map.insert(2, "banana");

    bool erased = map.erase(1);
    assert(erased == true);
    assert(map.size() == 1);
    assert(map.find(1) == map.end()); // Should no longer be found
    assert(map.find(2) != map.end()); // Banana should still be there
}

void test_erase_non_existing_element() {
    unordered_map<int, std::string> map;
    map.insert(1, "apple");

    bool erased = map.erase(99);
    assert(erased == false);
    assert(map.size() == 1);
}

void test_tombstone_recycling() {
    unordered_map<int, std::string> map;

    // Insert and delete to create a tombstone
    map.insert(1, "apple");
    map.erase(1);
    assert(map.size() == 0);

    // Insert a new element that might hash to the same spot
    map.insert(2, "banana");
    assert(map.size() == 1);
    assert(map.find(2) != map.end());
    assert(map[2] == "banana");
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
    assert(sum_keys == 6); // 1 + 2 + 3
}

void test_complex_types_memory_safety() {
    // This specifically tests if placement new and explicit destructors
    // are working correctly. If they aren't, this will likely segfault.
    unordered_map<int, std::vector<std::string>> map;

    map[1] = {"hello", "world"};
    map[2] = {"c++", "hash", "tables"};

    map.erase(1); // Triggers ~vector()
    map[2].push_back("rock"); // Triggers reallocation inside the vector

    assert(map.size() == 1);
    assert(map[2].size() == 4);
}

void test_reserve() {
    unordered_map<int, int> map;
    map.reserve(500);

    // If capacity is properly set, it should be at least (500 / loading_factor) rounded up to a power of 2
    assert(map.capacity() >= 512);

    // Ensure the map still works after reserve
    map.insert(1, 100);
    assert(map[1] == 100);
}

void test_contains() {
    unordered_map<int, int> map;
    map.insert(1, 1);
    map.insert(0, 2);
    map[-1] = 3;

    assert(map.contains(1));
    assert(map.contains(0));
    assert(map.contains(-1));
    assert(!map.contains(2));

    map.erase(0);
    map.erase(-1);
    assert(!map.contains(0));
    assert(!map.contains(-1));
    assert(map.contains(1));
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
    test_contains();

    std::cout << "All unordered_map tests passed!\n";
    return 0;
}