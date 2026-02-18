#include "../lib/unordered_map.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "=== Testing unordered_map<int,int> ===\n";

    // Create map with initial capacity 8
    unordered_map<int,int> map(8);

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

    // =============================
    // Iterator Tests
    // =============================
    std::cout << "Testing iterator...\n";

    int count = 0;
    long long sum_keys = 0;
    long long sum_values = 0;

    for (auto it = map.begin(); it != map.end(); ++it) {
        count++;
        sum_keys += *it;
        sum_values += &it;
    }

    assert(count == map.size());

    // Expected sums
    long long expected_key_sum = 0;
    long long expected_value_sum = 0;

    for (int i = 1; i <= 20; ++i) {
        expected_key_sum += i;
        if (i == 1) expected_value_sum += 101;
        else if (i == 4) expected_value_sum += 400;
        else if (i == 5) expected_value_sum += 500;
        else expected_value_sum += i * 100;
    }

    assert(sum_keys == expected_key_sum);
    assert(sum_values == expected_value_sum);

    std::cout << "Iterator traversal test passed\n";

    std::cout << "All tests passed!\n";
    return 0;
}
