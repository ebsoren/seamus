//
// Created by Aiden Mizhen on 2/14/26.
//

#include <iostream>
#include <cassert>
#include <sstream>
#include "../lib/string.h"

void test_view_constructors() {
    std::cout << "Testing string_view Constructors..." << std::endl;

    const char* raw_text = "search engine";

    // 1. Normal construction
    string_view sv1(raw_text, 13);
    assert(sv1.size() == 13);
    assert(sv1[0] == 's');
    assert(sv1[12] == 'e');

    // 2. Empty view construction
    string_view sv_empty(raw_text, 0);
    assert(sv_empty.size() == 0);
}

void test_element_access() {
    std::cout << "Testing Element Access..." << std::endl;

    const char* raw_text = "bazel";
    string_view sv(raw_text, 5);

    assert(sv[0] == 'b');
    assert(sv[1] == 'a');
    assert(sv[2] == 'z');
    assert(sv[3] == 'e');
    assert(sv[4] == 'l');
}

void test_substr() {
    std::cout << "Testing Substring (Slicing)..." << std::endl;

    const char* raw_text = "inverted_index_data";
    string_view full_view(raw_text, 19);

    // 1. Prefix slice
    string_view prefix = full_view.substr(0, 8); // "inverted"
    assert(prefix.size() == 8);
    assert(prefix[0] == 'i');
    assert(prefix[7] == 'd');

    // 2. Middle slice
    string_view middle = full_view.substr(9, 5); // "index"
    assert(middle.size() == 5);
    assert(middle[0] == 'i');
    assert(middle[4] == 'x');

    // 3. Suffix slice
    string_view suffix = full_view.substr(15, 4); // "data"
    assert(suffix.size() == 4);
    assert(suffix[0] == 'd');
    assert(suffix[3] == 'a');

    // 4. Zero-length slice
    string_view empty_slice = full_view.substr(5, 0);
    assert(empty_slice.size() == 0);

    // 5. Full slice (Identity slice)
    string_view identical = full_view.substr(0, 19);
    assert(identical.size() == 19);
}

void test_view_equality() {
    std::cout << "Testing View vs View Equality..." << std::endl;

    const char* raw1 = "token";
    const char* raw2 = "token"; // Different memory address, same chars
    const char* raw3 = "tokens";
    const char* raw4 = "apple";

    string_view v1(raw1, 5);
    string_view v2(raw2, 5);
    string_view v3(raw3, 6);
    string_view v4(raw4, 5);

    // 1. Identity and View vs View
    assert(v1 == v1);          // Identity (Same pointer, same length)
    assert(v1 == v2);          // Content match (Different pointer, same content)
    assert(!(v1 == v3));       // Length mismatch (Fast path failure)
    assert(!(v1 == v4));       // Content mismatch

    // 2. View vs C-String Literal (The new operators!)
    string_view sv_lit("view_literal", 12);
    assert(sv_lit == "view_literal");       // view == const char*
    assert("view_literal" == sv_lit);       // const char* == view (Symmetry)
    assert(!(sv_lit == "view_lit"));        // Length mismatch (too short)
    assert(!(sv_lit == "view_literal_"));   // Length mismatch (too long)
    assert(!(sv_lit == "view_boteral"));    // Content mismatch

    // Nullptr safety checks
    assert(!(sv_lit == nullptr));
    assert(!(nullptr == sv_lit));
}

void test_to_string_conversion() {
    std::cout << "Testing to_string() Bridge..." << std::endl;

    const char* raw_text = "convert_me_to_an_owned_string";
    string_view sv(raw_text, 29);

    // This forces the allocation of a new string object
    string owned_string = sv.to_string();

    assert(owned_string.size() == 29);
    assert(owned_string[0] == 'c');
    assert(owned_string[28] == 'g');

    // Verify using the heterogeneous operator we built!
    assert(owned_string == sv);
}

void test_view_ostream() {
    std::cout << "Testing std::ostream operator<<..." << std::endl;

    string_view sv("print_this_view", 15);
    std::stringstream ss;

    ss << sv;
    assert(ss.str() == "print_this_view");
}

int main() {
    try {
        test_view_constructors();
        test_element_access();
        test_substr();
        test_view_equality();
        test_to_string_conversion();
        test_view_ostream();

        std::cout << "\n-------------------------------------" << std::endl;
        std::cout << "STRING_VIEW TESTS PASSED SUCCESSFULLY" << std::endl;
        std::cout << "-------------------------------------" << std::endl;
    } catch (...) {
        std::cerr << "string_view tests failed with an unknown error." << std::endl;
        return 1;
    }
    return 0;
}