#include "../unordered_map.h"
#include <iostream>
#include <string>
#include <cassert>

// Simple hash and equality functions for int keys
uint64_t int_hash(int key) { return static_cast<uint64_t>(key); }

bool int_eq(int a, int b) { return a == b; }

int main() {
    std::cout << "=== Testing unordered_map<int,int> ===\n";

    // Create map with initial capacity 8
    unordered_map<int,int> map(8, int_hash, int_eq);

    // Test insert
    map.insert(1, 100);
    map.insert(2, 200);
    map.insert(3, 300);

    assert(map.size() == 3);
    std::cout << "Insert test passed\n";

    // Test find (existing keys)
    Slot<int,int>* s1 = map.find(1, 0);
    Slot<int,int>* s2 = map.find(2, 0);
    Slot<int,int>* s3 = map.find(3, 0);
    assert(s1 && s1->value == 100);
    assert(s2 && s2->value == 200);
    assert(s3 && s3->value == 300);
    std::cout << "Find existing keys test passed\n";

    // Test find (non-existing key inserts)
    Slot<int,int>* s4 = map.find(4, 400);
    assert(s4 && s4->value == 400);
    assert(map.size() == 4);
    std::cout << "Find non-existing key inserts test passed\n";

    // Test operator[]
    map[5] = 500;
    map[1] = 101; // overwrite existing
    assert(map[5] == 500);
    assert(map[1] == 101);
    assert(map.size() == 5);
    std::cout << "Operator[] test passed\n";

    // Test const find
    const auto& const_map = map;
    const Slot<int,int>* cs1 = const_map.find(2);
    const Slot<int,int>* cs2 = const_map.find(999); // should be nullptr
    assert(cs1 && cs1->value == 200);
    assert(cs2 == nullptr);
    std::cout << "Const find test passed\n";

    // Test rehash by inserting many elements
    for(int i=6; i<=20; ++i)
        map.insert(i, i*100);

    assert(map.size() == 20);
    std::cout << "Rehashing test passed (size = " << map.size() << ")\n";

    // Verify values after rehash
    for(int i=1; i<=20; ++i) {
        Slot<int,int>* s = map.find(i, 0);
        assert(s && (s->value == (i==1?101:i==4?400:i==5?500:i*100)));
    }
    std::cout << "Values verified after rehash\n";

    std::cout << "All tests passed!\n";
    return 0;
}
