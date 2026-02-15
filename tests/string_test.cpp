//
// Created by Aiden Mizhen on 2/13/26.
//
#include <iostream>
#include <cassert>
#include "../lib/string.h"


void test_constructors() {
    std::cout << "Testing Constructors..." << std::endl;

    // 1. C-string Short
    string s1("hello");
    assert(s1.size() == 5);
    assert(s1[0] == 'h');
    assert(s1[4] == 'o');

    // 2. C-string Long (15 chars is the first "long" string)
    string s2("this is a very long string");
    assert(s2.size() == 26);
    assert(s2[0] == 't');
    assert(s2[25] == 'g');
}

void test_integer_conversion() {
    std::cout << "Testing Integer Conversion..." << std::endl;

    // 1. Zero case (The "Trapdoor")
    string s_zero(0u);
    assert(s_zero.size() == 1);
    assert(s_zero[0] == '0');

    // 2. Small int
    string s_small(42u);
    assert(s_small.size() == 2);
    assert(s_small[0] == '4');
    assert(s_small[1] == '2');

    // 3. Max uint32_t (10 digits)
    string s_max(4294967295u);
    assert(s_max.size() == 10);
    assert(s_max[0] == '4');
    assert(s_max[9] == '5');
}

void test_equality() {
    std::cout << "Testing Equality..." << std::endl;

    string a("apple");
    string b("apple");
    string c("orange");
    string d("apples"); // different length

    assert(a == b);      // Value equality
    assert(a == a);      // Identity (Fast path)
    assert(!(a == c));   // Different content
    assert(!(a == d));   // Different length (Fast path)

    // Long string equality
    string long1("this is a long string that lives on the heap");
    string long2("this is a long string that lives on the heap");
    assert(long1 == long2);
}

void test_move_semantics() {
    std::cout << "Testing Move Semantics..." << std::endl;

    // 1. Move Constructor
    string original("move me!");
    size_t orig_len = original.size();

    // Move 'original' into 'moved_to'
    string moved_to(static_cast<string&&>(original));

    assert(moved_to.size() == orig_len);
    assert(moved_to[0] == 'm');
    assert(original.size() == 0); // Original should be hollowed out

    // 2. Move Assignment
    string s_assign("I will be replaced");
    string s_source("New content");

    s_assign = static_cast<string&&>(s_source);

    assert(s_assign.size() == 11);
    assert(s_assign[0] == 'N');
    assert(s_source.size() == 0);
}

void test_sso_boundaries() {
    std::cout << "Testing SSO Boundaries..." << std::endl;

    // 14 characters = Short
    string s14("12345678901234");
    assert(s14.size() == 14);

    // 15 characters = Long
    string s15("123456789012345");
    assert(s15.size() == 15);
}

void test_memory_stability() {
    std::cout << "Testing Memory Stability (Check for crashes)..." << std::endl;

    // Rapidly creating and destroying long strings
    // If your destructor is wrong, this would leak/crash
    for (int i = 0; i < 1000; ++i) {
        string temp("Just a string to test if the heap allocation/deallocation is stable");
    }
}

void test_reassignment_leak() {
    std::cout << "Testing Reassignment (Memory Leak Check)..." << std::endl;
    string s("This is a very long string that allocates memory");
    // If Move Assignment doesn't delete the old buffer, this leaks:
    s = string("Short");
    assert(s.size() == 5);
}

void test_self_move() {
    std::cout << "Testing Self-Move Guard..." << std::endl;
    string s("I am long enough to be on the heap");
    s = static_cast<string&&>(s);
    assert(s.size() > 14); // Should still be valid
}

void test_mixed_equality() {
    std::cout << "Testing Pointer-Aliasing Equality..." << std::endl;
    string short_s("abc");
    string long_s("abc_this_is_very_long");
    assert(!(short_s == long_s)); // Should fail fast on length
}

void test_moved_from_state() {
    std::cout << "Testing Moved-from State Robustness..." << std::endl;
    string s1("I am going away");
    string s2 = static_cast<string&&>(s1);

    // s1 is now empty. This shouldn't crash.
    assert(s1.size() == 0);
    assert(s1 == string(""));
}


#include <sstream> // Required for test_ostream

void test_pointer_length_constructor() {
    std::cout << "Testing Pointer+Length Constructor..." << std::endl;

    const char* buffer = "hello world! this is extra data";

    // Slice exactly 5 characters ("hello") - Should trigger Short String
    string s_short(buffer, 5);
    assert(s_short.size() == 5);
    assert(s_short == string("hello"));

    // Slice exactly 18 characters - Should trigger Heap String
    string s_long(buffer, 18);
    assert(s_long.size() == 18);
    assert(s_long == string("hello world! this "));
}

void test_str_view_generation() {
    std::cout << "Testing str_view Generation..." << std::endl;

    string short_str("search");
    string long_str("inverted_index_engine");

    // 1. View of a short string
    string_view v1 = short_str.str_view(0, 6);
    assert(v1.size() == 6);
    assert(v1[0] == 's' && v1[5] == 'h');

    // 2. View of a long string (slice the middle)
    string_view v2 = long_str.str_view(9, 5); // "index"
    assert(v2.size() == 5);
    assert(v2[0] == 'i' && v2[4] == 'x');

    // 3. Zero-length view
    string_view v_empty = short_str.str_view(0, 0);
    assert(v_empty.size() == 0);
}

void test_heterogeneous_equality() {
    std::cout << "Testing string == string_view Equality..." << std::endl;

    string s("bazel_build");
    string_view exact_view = s.str_view(0, 11);
    string_view sub_view = s.str_view(0, 5); // "bazel"
    string diff("bazel");

    // 1. string == view (Perfect match, pointer identity fast-path)
    assert(s == exact_view);

    // 2. view == string (Testing symmetry)
    assert(exact_view == s);

    // 3. string == view (Content match, different pointers)
    assert(diff == sub_view);

    // 4. string != view (Length mismatch)
    assert(!(s == sub_view));
}

void test_advanced_move_assignments() {
    std::cout << "Testing Advanced Move Assignments..." << std::endl;

    // 1. Heap to Heap (Must delete old heap)
    string h1("this is my first heap string");
    string h2("this is my second heap string");
    h1 = static_cast<string&&>(h2);
    assert(h1.size() == 29); // Size of second string
    assert(h1[0] == 't');

    // 2. Short to Heap (Must delete old heap, then copy short buffer)
    string h3("I am currently on the heap");
    string s_short("tiny");
    h3 = static_cast<string&&>(s_short);
    assert(h3.size() == 4);
    assert(h3[0] == 't');
}

void test_ostream() {
    std::cout << "Testing std::ostream operator<<..." << std::endl;

    string s_short("C++");
    string s_long("Systems Programming");

    std::stringstream ss;

    ss << s_short;
    assert(ss.str() == "C++");

    // Clear the stream
    ss.str("");

    ss << s_long;
    assert(ss.str() == "Systems Programming");
}


int main() {
    try {
        test_constructors();
        test_integer_conversion();
        test_equality();
        test_move_semantics();
        test_sso_boundaries();
        test_memory_stability();
        test_self_move();
        test_mixed_equality();
        test_moved_from_state();
        test_pointer_length_constructor();
        test_str_view_generation();
        test_heterogeneous_equality();
        test_advanced_move_assignments();
        test_ostream();

        std::cout << "\n--------------------------" << std::endl;
        std::cout << "ALL TESTS PASSED SUCCESSFULLY" << std::endl;
        std::cout << "--------------------------" << std::endl;
    } catch (...) {
        std::cerr << "Tests failed with an unknown error." << std::endl;
        return 1;
    }
    return 0;
}