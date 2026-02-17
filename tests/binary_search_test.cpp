#include <cassert>
#include <vector>
#include <array>
#include <string>
#include <iostream>
#include "../lib/algorithm.h"

void test_empty_range() {
    std::vector<int> v;
    int x = 5;
    assert(!binary_search(v.begin(), v.end(), x));
}

void test_single_element_found() {
    std::vector<int> v = {10};
    int x = 10;
    assert(binary_search(v.begin(), v.end(), x));
}

void test_single_element_not_found() {
    std::vector<int> v = {10};
    int x = 5;
    assert(!binary_search(v.begin(), v.end(), x));
}

void test_multiple_elements_found() {
    std::vector<int> v = {1, 3, 5, 7, 9, 11};
    int x = 7;
    assert(binary_search(v.begin(), v.end(), x));
}

void test_multiple_elements_not_found() {
    std::vector<int> v = {1, 3, 5, 7, 9, 11};
    int x = 6;
    assert(!binary_search(v.begin(), v.end(), x));
}

void test_first_element() {
    std::vector<int> v = {2, 4, 6, 8, 10};
    int x = 2;
    assert(binary_search(v.begin(), v.end(), x));
}

void test_last_element() {
    std::vector<int> v = {2, 4, 6, 8, 10};
    int x = 10;
    assert(binary_search(v.begin(), v.end(), x));
}

void test_all_elements() {
    std::vector<int> v = {-5, -2, 0, 3, 9, 14};
    for (int x : v) {
        assert(binary_search(v.begin(), v.end(), x));
    }
}

void test_duplicates_present() {
    std::vector<int> v = {1, 2, 2, 2, 3, 4};
    int x = 2;
    assert(binary_search(v.begin(), v.end(), x));
}

void test_value_smaller_than_all() {
    std::vector<int> v = {10, 20, 30};
    int x = 5;
    assert(!binary_search(v.begin(), v.end(), x));
}

void test_value_larger_than_all() {
    std::vector<int> v = {10, 20, 30};
    int x = 40;
    assert(!binary_search(v.begin(), v.end(), x));
}

void test_array_iterator() {
    std::array<int, 5> a = {1, 3, 5, 7, 9};
    int x = 7;
    assert(binary_search(a.begin(), a.end(), x));
}

void test_string_type() {
    std::vector<std::string> v = {"apple", "banana", "cherry", "date"};
    std::string x = "cherry";
    assert(binary_search(v.begin(), v.end(), x));
}

void test_string_not_found() {
    std::vector<std::string> v = {"apple", "banana", "cherry", "date"};
    std::string x = "fig";
    assert(!binary_search(v.begin(), v.end(), x));
}

void test_subrange() {
    std::vector<int> v = {1, 3, 5, 7, 9, 11};
    int x = 5;
    assert(binary_search(v.begin() + 1, v.begin() + 4, x)); // {3,5,7}
}

int main() {
    test_empty_range();
    test_single_element_found();
    test_single_element_not_found();
    test_multiple_elements_found();
    test_multiple_elements_not_found();
    test_first_element();
    test_last_element();
    test_all_elements();
    test_duplicates_present();
    test_value_smaller_than_all();
    test_value_larger_than_all();
    test_array_iterator();
    test_string_type();
    test_string_not_found();
    test_subrange();

    std::cout << "All binary_search tests passed!\n";
}
