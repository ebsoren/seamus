#include "../lib/deque.h"
#include <iostream>
#include <cassert>
#include <deque>

#define TEST_CASE(name) void name()

#define RUN_TEST(name) \
    std::cout << "Running test: " << #name << "..." << std::endl; \
    name(); \
    std::cout << "Test " << #name << " passed!" << std::endl;

TEST_CASE(test_default_constructor_and_empty) {
    deque<int> d;
    assert(d.empty());
    assert(d.size() == 0);
}

TEST_CASE(test_push_back_basic) {
    deque<int> d;
    d.push_back(10);
    assert(!d.empty());
    assert(d.size() == 1);
    assert(*d.front() == 10);
    assert(*d.back() == 10);

    d.push_back(20);
    assert(d.size() == 2);
    assert(*d.front() == 10);
    assert(*d.back() == 20);
}

TEST_CASE(test_push_front_basic) {
    deque<int> d;
    d.push_front(10);
    assert(!d.empty());
    assert(d.size() == 1);
    assert(*d.front() == 10);
    assert(*d.back() == 10);

    d.push_front(20);
    assert(d.size() == 2);
    assert(*d.front() == 20);
    assert(*d.back() == 10);
}

TEST_CASE(test_pop_back_basic) {
    deque<int> d;
    d.push_back(10);
    d.push_back(20);
    d.pop_back();
    assert(d.size() == 1);
    assert(*d.front() == 10);
    assert(*d.back() == 10);
    d.pop_back();
    assert(d.empty());
}

TEST_CASE(test_pop_front_basic) {
    deque<int> d;
    d.push_back(10);
    d.push_back(20);
    d.pop_front();
    assert(d.size() == 1);
    assert(*d.front() == 20);
    assert(*d.back() == 20);
    d.pop_front();
    assert(d.empty());
}

TEST_CASE(test_mixed_push_pop) {
    deque<int> d;
    d.push_back(10);
    d.push_front(20);
    d.push_back(30);
    d.push_front(40); // deque is now: 40, 20, 10, 30
    assert(d.size() == 4);
    assert(*d.front() == 40);
    assert(*d.back() == 30);

    d.pop_front(); // deque is now: 20, 10, 30
    assert(d.size() == 3);
    assert(*d.front() == 20);

    d.pop_back(); // deque is now: 20, 10
    assert(d.size() == 2);
    assert(*d.back() == 10);
}

TEST_CASE(test_indexing_operator) {
    deque<int> d;
    for (int i = 0; i < 5; ++i) {
        d.push_back(i);
    }
    for (int i = 0; i < 5; ++i) {
        assert(d[i] == i);
    }

    d.push_front(100);
    d.push_front(200);
    // deque is now: 200, 100, 0, 1, 2, 3, 4
    assert(d[0] == 200);
    assert(d[1] == 100);
    assert(d[2] == 0);
    assert(d[6] == 4);
}

TEST_CASE(test_fill_internal_array) {
    deque<int> d;
    // The internal array size is 512 bytes / sizeof(int) = 128
    int internal_array_size = 512 / sizeof(int);
    for (int i = 0; i < internal_array_size; ++i) {
        d.push_back(i);
    }
    assert(d.size() == internal_array_size);
    assert(*d.front() == 0);
    assert(*d.back() == internal_array_size - 1);

    // This should trigger a new block allocation
    d.push_back(internal_array_size);
    assert(d.size() == internal_array_size + 1);
    assert(*d.back() == internal_array_size);
    assert(d[internal_array_size] == internal_array_size);

    // This should trigger a new block allocation at the front
    d.push_front(-1);
    assert(d.size() == internal_array_size + 2);
    assert(*d.front() == -1);
    assert(d[0] == -1);
    assert(d[1] == 0);
}

TEST_CASE(test_reallocation_and_stress) {
    deque<int> my_d;
    std::deque<int> std_d;

    const int num_operations = 10000;

    // Phase 1: Growth
    for (int i = 0; i < num_operations; ++i) {
        int val = rand();
        int op = rand() % 4;

        if (op == 0) {
            my_d.push_back(val);
            std_d.push_back(val);
        } else if (op == 1) {
            my_d.push_front(val);
            std_d.push_front(val);
        } else if (op == 2 && !std_d.empty()) {
            my_d.pop_back();
            std_d.pop_back();
        } else if (op == 3 && !std_d.empty()) {
            my_d.pop_front();
            std_d.pop_front();
        }

        assert(my_d.size() == std_d.size());
        if (!std_d.empty()) {
            assert(*my_d.front() == std_d.front());
            assert(*my_d.back() == std_d.back());
        }
    }

    // Verification after phase 1
    assert(my_d.size() == std_d.size());
    for (size_t i = 0; i < std_d.size(); ++i) {
        assert(my_d[i] == std_d[i]);
    }

    // Phase 2: Shrinkage
    while(!my_d.empty()){
        int op = rand() % 2;
        if (op == 0) {
            my_d.pop_back();
            std_d.pop_back();
        } else {
            my_d.pop_front();
            std_d.pop_front();
        }
        assert(my_d.size() == std_d.size());
        if (!std_d.empty()) {
            assert(*my_d.front() == std_d.front());
            assert(*my_d.back() == std_d.back());
        }
    }

    assert(my_d.empty());
    assert(std_d.empty());

    // Phase 3: Regrowth
    for (int i = 0; i < num_operations / 2; ++i) {
        int val = rand();
        if (i % 2 == 0) {
            my_d.push_back(val);
            std_d.push_back(val);
        } else {
            my_d.push_front(val);
            std_d.push_front(val);
        }
    }

    // Final verification
    assert(my_d.size() == std_d.size());
    for (size_t i = 0; i < std_d.size(); ++i) {
        assert(my_d[i] == std_d[i]);
    }
}


int main() {
    RUN_TEST(test_default_constructor_and_empty);
    RUN_TEST(test_push_back_basic);
    RUN_TEST(test_push_front_basic);
    RUN_TEST(test_pop_back_basic);
    RUN_TEST(test_pop_front_basic);
    RUN_TEST(test_mixed_push_pop);
    RUN_TEST(test_indexing_operator);
    RUN_TEST(test_fill_internal_array);
    RUN_TEST(test_reallocation_and_stress);

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
