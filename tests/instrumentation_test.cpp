#include "../crawler/crawler_instrumentation.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include <vector>


void test_instrumentation_basic() {
    printf("---- test_instrumentation_basic ----\n");

    size_t num_workers = 4;
    size_t drain_interval = 1;
    CrawlerInstrumentation instr(num_workers, drain_interval);
    instr.start();

    // Each fake worker submits 10 updates per batch, 10 batches with sleeps in between
    // so we can observe the instrumentation log accumulating over multiple drain cycles
    std::vector<std::thread> workers;
    for (size_t w = 0; w < num_workers; w++) {
        workers.push_back(std::thread([&instr, w]() {
            for (int batch = 0; batch < 10; batch++) {
                for (int i = 0; i < 10; i++) {
                    int idx = batch * 10 + i;
                    instr.submit(w, {MetricType::DOCUMENTS_CRAWLED_ACCUMULATE, 1.0, 0});
                    instr.submit(w, {MetricType::PAGE_LENGTH_AVERAGE, 500.0 + idx, 1});
                    instr.submit(w, {MetricType::PAGE_PRIORITY_AVERAGE, 3.0, 1});
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }));
    }

    for (auto& t : workers) t.join();

    // Wait for final drain cycle
    std::this_thread::sleep_for(std::chrono::seconds(drain_interval + 1));

    // Verify accumulated state
    uint64_t docs = instr.get_documents_crawled();
    double avg_len = instr.get_avg_page_length();
    double avg_pri = instr.get_avg_page_priority();

    printf("  docs crawled:     %llu (expected %zu)\n", docs, num_workers * 100);
    printf("  avg page length:  %.1f\n", avg_len);
    printf("  avg page priority: %.2f (expected 3.00)\n", avg_pri);

    assert(docs == num_workers * 100);
    assert(avg_pri > 2.99 && avg_pri < 3.01);
    // Each worker submits lengths 500..599, average = 549.5
    assert(avg_len > 549.0 && avg_len < 550.0);

    printf("PASS\n");
}


void test_instrumentation_concurrent_stress() {
    printf("---- test_instrumentation_concurrent_stress ----\n");

    size_t num_workers = 16;
    size_t drain_interval = 1;
    CrawlerInstrumentation instr(num_workers, drain_interval);
    instr.start();

    size_t updates_per_worker = 10000;
    std::vector<std::thread> workers;
    for (size_t w = 0; w < num_workers; w++) {
        workers.push_back(std::thread([&instr, w, updates_per_worker]() {
            for (size_t i = 0; i < updates_per_worker; i++) {
                instr.submit(w, {MetricType::DOCUMENTS_CRAWLED_ACCUMULATE, 1.0, 0});
                instr.submit(w, {MetricType::PAGE_LENGTH_AVERAGE, 1024.0, 1});
                instr.submit(w, {MetricType::PAGE_PRIORITY_AVERAGE, 5.0, 1});
            }
        }));
    }

    for (auto& t : workers) t.join();

    // Wait for drain
    std::this_thread::sleep_for(std::chrono::seconds(drain_interval + 1));

    uint64_t expected_docs = num_workers * updates_per_worker;
    uint64_t docs = instr.get_documents_crawled();
    double avg_len = instr.get_avg_page_length();
    double avg_pri = instr.get_avg_page_priority();

    printf("  docs crawled:      %llu (expected %llu)\n", docs, expected_docs);
    printf("  avg page length:   %.1f (expected 1024.0)\n", avg_len);
    printf("  avg page priority: %.2f (expected 5.00)\n", avg_pri);

    assert(docs == expected_docs);
    assert(avg_len > 1023.9 && avg_len < 1024.1);
    assert(avg_pri > 4.99 && avg_pri < 5.01);

    printf("PASS\n");
}


int main() {
    printf("\n===== RUNNING INSTRUMENTATION TESTS =====\n\n");
    test_instrumentation_basic();
    test_instrumentation_concurrent_stress();
    printf("\n===== ALL INSTRUMENTATION TESTS PASSED =====\n");
    return 0;
}
