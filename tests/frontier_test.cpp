#include "../lib/Frontier.h"
#include <cassert>
#include <iostream>

using std::cout;

void test_string_basic() {
    string a("hello");
    string b("world");

    assert(a.size() == 5);
    assert(a[0] == 'h');
    assert(a.str_view(0, 5) == "hello");

    cout << "test_string_basic passed\n";
}

void test_string_move() {
    string a("hello");

    string b = std::move(a);  // move constructor

    assert(b == "hello");
    assert(b.size() == 5);

    cout << "test_string_move passed\n";
}

void test_string_join() {
    string s = string::join("-", "a", "b", "c");

    assert(s == "a-b-c");

    cout << "test_string_join passed\n";
}

void test_string_view() {
    string s("https://example.com/path");

    auto view = s.str_view(8, 7);

    assert(view == "example");

    string rebuilt = view.to_string();

    assert(rebuilt == "example");

    cout << "test_string_view passed\n";
}

void test_frontier_push_front_pop() {
    Frontier f(1);

    string url("https://example.com");

    f.push(std::move(url), 0);

    assert(f.size() == 1);

    CrawledItem item = f.front();
    assert(item.url.str_view(0, item.url.size()) == "https://example.com");

    f.pop();
    assert(f.size() == 0);

    cout << "test_frontier_push_front_pop passed\n";
}

void test_frontier_move_only() {
    Frontier f(1);

    f.push(string("https://a.com"), 0);
    f.push(string("https://b.com"), 0);

    assert(f.size() == 2);

    CrawledItem item1 = f.front();
    f.pop();

    CrawledItem item2 = f.front();

    assert(!(item1.url == item2.url));

    cout << "test_frontier_move_only passed\n";
}

void test_priority_ranking() {
    Frontier f(1);

    f.push(string("https://harvard.edu"), 0);  // high
    f.push(string("http://example123.biz"), 50); // low
    f.push(string("https://mit.edu"), 1);       // high

    CrawledItem first = f.front();

    assert(first.url.str_view(0, first.url.size()) == "https://harvard.edu"
        || first.url.str_view(0, first.url.size()) == "https://mit.edu");

    cout << "test_priority_ranking passed\n";
}

void test_seed_distance_effect() {
    Frontier f(1);

    f.push(string("https://a.com"), 0);
    f.push(string("https://a.com"), 100);

    CrawledItem first = f.front();

    assert(first.seed_list_dist == 0);

    cout << "test_seed_distance_effect passed\n";
}

void test_many_push_pop() {
    Frontier f(1);

    for (int i = 0; i < 5000; i++) {
        f.push(string::join("", "https://site", string(i), ".com"), i % 10);
    }

    assert(f.size() == 5000);

    for (int i = 0; i < 5000; i++) {
        f.pop();
    }

    assert(f.size() == 0);

    cout << "test_many_push_pop passed\n";
}

void test_url_parsing() {
    Frontier f(1);

    f.push(string("https://sub.domain123.example.com/path/to/page?x=1"), 0);

    CrawledItem item = f.front();

    string_view view = item.url.str_view(8, item.url.size() - 8);

    assert(view == "sub.domain123.example.com/path/to/page?x=1");

    cout << "test_url_parsing passed\n";
}

void test_edge_case_empty_frontier() {
    Frontier f(1);

    try {
        f.front();
        assert(false); // should not reach
    } catch (...) {
        cout << "test_edge_case_empty_frontier passed\n";
    }
}

void test_digits_and_subdomains_penalty() {
    Frontier f(1);

    f.push(string("https://a.b.c.d.example123.com"), 0);
    f.push(string("https://example.com"), 0);

    CrawledItem first = f.front();

    // cleaner URL should rank higher
    assert(first.url == "https://example.com");

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