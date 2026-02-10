#include <iostream>
#include <cassert>
#include "../priority_queue.h"   // update path if needed

// test 1: default constructor + empty check
void test_default_empty() {
    priority_queue<int> pq;
    assert(pq.empty());
    assert(pq.size() == 0);
}

// test 2: push + front ordering (max heap default)
void test_push_front_order() {
    priority_queue<int> pq;
    pq.push(5);
    pq.push(1);
    pq.push(9);
    pq.push(3);

    assert(!pq.empty());
    assert(pq.front() == 9);  // max heap => largest on top
    assert(pq.size() == 4);
}

// test 3: pop maintains correct order
void test_pop_order() {
    priority_queue<int> pq;
    pq.push(2);
    pq.push(7);
    pq.push(4);

    assert(pq.front() == 7);
    pq.pop();

    assert(pq.front() == 4);
    pq.pop();

    assert(pq.front() == 2);
    pq.pop();

    assert(pq.empty());
}

// test 4: custom comparator => min heap
void test_custom_comparator() {
    struct greater {
        constexpr bool operator()(int a, int b) const {
            return a > b;
        }
    };

    priority_queue<int, vector<int>, greater> pq;
    pq.push(5);
    pq.push(2);
    pq.push(8);

    // min heap behavior
    assert(pq.front() == 2);
    pq.pop();
    assert(pq.front() == 5);
}

// test 5: constructor from range
void test_range_constructor() {
    int arr[] = {3, 1, 4, 1, 5, 9};

    priority_queue<int> pq(arr, arr + 6);

    assert(pq.front() == 9);
    assert(pq.size() == 6);
}

int main() {
    test_default_empty();
    test_push_front_order();
    test_pop_order();
    test_custom_comparator();
    test_range_constructor();

    std::cout << "all tests passed!\n";
    return 0;
}
