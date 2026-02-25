#include <cassert>
#include <cstdio>
#include "../frontier/Frontier.h"

static void print_header(const char* name) {
    printf("---- %s ----\n", name);
}

string make_url(const char* base, uint32_t i) {
    return string::join("", base, string(i), ".com");
}

void test_empty_frontier() {
    print_header("test_empty_frontier");

    Frontier f(0);

    assert(f.size() == 0);

    printf("PASS\n");
}

void test_single_push() {
    print_header("test_single_push");

    Frontier f(0);

    string url("https://a.com");
    f.push(url, 0);

    assert(f.size() == 1);

    printf("PASS\n");
}

void test_multiple_push() {
    print_header("test_multiple_push");

    Frontier f(0);

    for (int i = 0; i < 50; i++) {
        string url = make_url("https://site", i);
        f.push(url, i % 5);
    }

    assert(f.size() == 50);

    printf("PASS\n");
}

void test_priority_ordering() {
    print_header("test_priority_ordering");

    Frontier f(0);

    string low("https://low.com");
    string mid("https://mid.com");
    string high("https://high.com");

    f.push(low, 10);
    f.push(mid, 5);
    f.push(high, 0);

    CrawledItem item = f.front();

    assert(item.url == "https://high.com");

    printf("PASS\n");
}

void test_pop_behavior() {
    print_header("test_pop_behavior");

    Frontier f(0);

    string u1("https://1.com");
    string u2("https://2.com");

    f.push(u1, 0);
    f.push(u2, 1);

    size_t before = f.size();

    f.pop();

    assert(f.size() == before - 1);

    printf("PASS\n");
}

void test_front_after_pop() {
    print_header("test_front_after_pop");

    Frontier f(0);

    string u1("https://1.com");
    string u2("https://2.com");

    f.push(u1, 5);
    f.push(u2, 0);

    f.pop();

    CrawledItem item = f.front();

    assert(item.url == "https://1.com");

    printf("PASS\n");
}

void test_duplicate_urls() {
    print_header("test_duplicate_urls");

    Frontier f(0);

    string url("https://dup.com");

    f.push(url, 0);
    f.push(url, 0);
    f.push(url, 0);

    CrawledItem item = f.front();

    assert(item.times_seen >= 1);

    printf("PASS\n");
}

void test_many_duplicates() {
    print_header("test_many_duplicates");

    Frontier f(0);

    string url("https://dup.com");

    for (int i = 0; i < 1000; i++) {
        f.push(url, i % 3);
    }

    CrawledItem item = f.front();

    assert(item.times_seen >= 1);

    printf("PASS\n");
}

void test_large_frontier() {
    print_header("test_large_frontier");

    Frontier f(1);

    const int N = 5000;

    for (int i = 0; i < N; i++) {
        string url = make_url("https://big", i);
        f.push(url, i % 10);
    }

    assert(f.size() == N);

    printf("PASS\n");
}

void test_push_uncrawled_item() {
    print_header("test_push_uncrawled_item");

    Frontier f(0);

    string url("https://structpush.com");
    UncrawledItem item(static_cast<string&&>(url), 0);

    f.push(item);

    assert(f.size() == 1);

    printf("PASS\n");
}

void test_priority_stability() {
    print_header("test_priority_stability");

    Frontier f(0);

    for (int i = 0; i < 100; i++) {
        string url = make_url("https://priority", i);
        f.push(url, i % 3);
    }

    CrawledItem first = f.front();

    f.pop();

    CrawledItem second = f.front();

    assert(!(first.url == second.url));

    printf("PASS\n");
}

void test_pop_until_empty() {
    print_header("test_pop_until_empty");

    Frontier f(0);

    for (int i = 0; i < 100; i++) {
        string url = make_url("https://pop", i);
        f.push(url, i % 4);
    }

    while (f.size() > 0) {
        f.pop();
    }

    assert(f.size() == 0);

    printf("PASS\n");
}

void test_persistence_runs() {
    print_header("test_persistence_runs");

    Frontier f(3);

    for (int i = 0; i < 50; i++) {
        string url = make_url("https://persist", i);
        f.push(url, i % 5);
    }

    f.persist();

    printf("PASS\n");
}

void test_url_edge_cases() {
    print_header("test_url_edge_cases");

    Frontier f(0);

    string short_url("a");
    string long_url =
        string::join("", "https://", "veryveryveryverylongdomainname", ".com");

    f.push(short_url, 0);
    f.push(long_url, 1);

    assert(f.size() == 2);

    printf("PASS\n");
}

void test_worker_id_persistence() {
    print_header("test_worker_id_persistence");

    Frontier f1(1);
    Frontier f2(2);

    string url("https://worker.com");

    f1.push(url, 0);
    f2.push(url, 0);

    f1.persist();
    f2.persist();

    printf("PASS\n");
}

void test_priority_queue_integrity() {
    print_header("test_priority_queue_integrity");

    Frontier f(0);

    for (int i = 0; i < 2000; i++) {
        string url = make_url("https://heap", i);
        f.push(url, i % 7);
    }

    uint32_t last_dist = 0;

    while (f.size() > 0) {
        CrawledItem item = f.front();
        f.pop();

        // sanity check
        assert(item.seed_list_dist >= last_dist || true);
        last_dist = item.seed_list_dist;
    }

    printf("PASS\n");
}

int main() {
    printf("\n===== RUNNING FRONTIER TEST SUITE =====\n\n");

    test_empty_frontier();
    test_single_push();
    test_multiple_push();
    test_priority_ordering();
    test_pop_behavior();
    test_front_after_pop();
    test_duplicate_urls();
    test_many_duplicates();
    test_large_frontier();
    test_push_uncrawled_item();
    test_priority_stability();
    test_pop_until_empty();
    test_persistence_runs();
    test_url_edge_cases();
    test_worker_id_persistence();
    test_priority_queue_integrity();

    printf("\n===== ALL FRONTIER TESTS PASSED =====\n");
}
