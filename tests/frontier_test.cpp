#include "../lib/Frontier.h"
#include <iostream>
#include <cassert>

#define PRINT_AND_ASSERT(cond, expected, actual)            \
    do {                                                    \
        if (!(cond)) {                                      \
            std::cerr << "[TEST FAILED]\n";                 \
            std::cerr << "  Condition: " << #cond << "\n";  \
            std::cerr << "  Expected: " << (expected) << "\n"; \
            std::cerr << "  Actual:   " << (actual) << "\n";   \
            std::cerr << "  File: " << __FILE__ << "\n";    \
            std::cerr << "  Line: " << __LINE__ << "\n";    \
            assert(cond);                                   \
        }                                                   \
    } while (0)

using std::cout;

void test_string_basic() {
    string a("hello");
    string b("world");

    PRINT_AND_ASSERT(a.size() == 5, 5, a.size());
    PRINT_AND_ASSERT(a[0] == 'h', 'h', a[0]);
    PRINT_AND_ASSERT(a.str_view(0, 5) == "hello", "hello", a.str_view(0, 5));

    cout << "test_string_basic passed\n";
}

void test_string_move() {
    string a("hello");
    string b = std::move(a);

    PRINT_AND_ASSERT(b == "hello", "hello", b);
    PRINT_AND_ASSERT(b.size() == 5, 5, b.size());

    cout << "test_string_move passed\n";
}

void test_string_join() {
    string s = string::join("-", "a", "b", "c");

    PRINT_AND_ASSERT(s == "a-b-c", "a-b-c", s);

    cout << "test_string_join passed\n";
}

void test_string_view() {
    string s("https://example.com/path");

    auto view = s.str_view(8, 7);

    PRINT_AND_ASSERT(view == "example", "example", view);

    string rebuilt = view.to_string();

    PRINT_AND_ASSERT(rebuilt == "example", "example", rebuilt);

    cout << "test_string_view passed\n";
}

void test_frontier_push_front_pop() {
    Frontier f(1);

    string url("https://example.com");

    f.push(std::move(url), 0);

    PRINT_AND_ASSERT(f.size() == 1, 1, f.size());

    CrawledItem item = f.front();
    PRINT_AND_ASSERT(item.url.str_view(0, item.url.size()) == "https://example.com",
                     "https://example.com",
                     item.url.str_view(0, item.url.size()));

    f.pop();
    PRINT_AND_ASSERT(f.size() == 0, 0, f.size());

    cout << "test_frontier_push_front_pop passed\n";
}

void test_frontier_move_only() {
    Frontier f(1);

    f.push(string("https://a.com"), 0);
    f.push(string("https://b.com"), 0);

    PRINT_AND_ASSERT(f.size() == 2, 2, f.size());

    CrawledItem item1 = f.front();
    f.pop();

    CrawledItem item2 = f.front();

    PRINT_AND_ASSERT(!(item1.url == item2.url), "different URLs", "equal URLs");

    cout << "test_frontier_move_only passed\n";
}

void test_priority_ranking() {
    Frontier f(1);

    f.push(string("https://harvard.edu"), 0);
    f.push(string("http://example123.biz"), 50);
    f.push(string("https://mit.edu"), 1);

    CrawledItem first = f.front();

    PRINT_AND_ASSERT(
        (first.url.str_view(0, first.url.size()) == "https://harvard.edu" ||
         first.url.str_view(0, first.url.size()) == "https://mit.edu"),
        "high-quality URL",
        first.url.str_view(0, first.url.size())
    );

    cout << "test_priority_ranking passed\n";
}

void test_seed_distance_effect() {
    Frontier f(1);

    f.push(string("https://a.com"), 0);
    f.push(string("https://a.com"), 100);

    CrawledItem first = f.front();
    f.pop();
    CrawledItem second = f.front();


    PRINT_AND_ASSERT(second.seed_list_dist == 100, 100, second.seed_list_dist);
    PRINT_AND_ASSERT(first.seed_list_dist == 0, 0, first.seed_list_dist);

    cout << "test_seed_distance_effect passed\n";
}

void test_many_push_pop() {
    Frontier f(1);

    for (int i = 0; i < 5000; i++) {
        f.push(string::join("", "https://site", string(i), ".com"), i % 10);
    }

    PRINT_AND_ASSERT(f.size() == 5000, 5000, f.size());

    for (int i = 0; i < 5000; i++) {
        f.pop();
    }

    PRINT_AND_ASSERT(f.size() == 0, 0, f.size());

    cout << "test_many_push_pop passed\n";
}

void test_url_parsing() {
    Frontier f(1);

    f.push(string("https://sub.domain123.example.com/path/to/page?x=1"), 0);

    CrawledItem item = f.front();

    string_view view = item.url.str_view(8, item.url.size() - 8);

    PRINT_AND_ASSERT(view == "sub.domain123.example.com/path/to/page?x=1",
                     "parsed URL",
                     view);

    cout << "test_url_parsing passed\n";
}

void test_edge_case_empty_frontier() {
    Frontier f(1);

    try {
        f.front();
        PRINT_AND_ASSERT(false, "exception expected", "no exception");
    } catch (...) {
        cout << "test_edge_case_empty_frontier passed\n";
    }
}

void test_digits_and_subdomains_penalty() {
    Frontier f(1);

    f.push(string("https://a.b.c.d.example123.com"), 0);
    f.push(string("https://example.com"), 0);

    CrawledItem first = f.front();

    PRINT_AND_ASSERT(first.url == "https://example.com",
                     "https://example.com",
                     first.url);

    cout << "test_digits_and_subdomains_penalty passed\n";
}

int main() {
    test_string_basic();
    test_string_move();
    test_string_join();
    test_string_view();
    test_frontier_push_front_pop();
    test_frontier_move_only();
    test_priority_ranking();
    test_seed_distance_effect();
    test_many_push_pop();
    test_url_parsing();
    test_edge_case_empty_frontier();
    test_digits_and_subdomains_penalty();

    cout << "\nALL TESTS PASSED\n";
}