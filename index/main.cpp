#include "Index.h"
#include <atomic>
#include <chrono>
#include <thread>
#include "lib/consts.h"
#include "lib/logger.h"


// Periodic progress reporter: prints total docs indexed, interval rate, and
// running average rate. Stops when `running` flips to false.
inline void progress_reporter(std::atomic<bool>& running) {
    auto start = std::chrono::steady_clock::now();
    uint64_t prev = 0;
    const uint64_t interval_sec = INSTRUMENTATION_INTERVAL_SEC;
    while (running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::seconds(interval_sec));
        if (!running.load(std::memory_order_relaxed)) break;

        uint64_t cur = g_indexed_docs.load(std::memory_order_relaxed);
        uint64_t delta = cur - prev;
        prev = cur;

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
        long hrs = elapsed / 3600;
        long mins = (elapsed % 3600) / 60;
        long secs = elapsed % 60;
        double rate = static_cast<double>(delta) / static_cast<double>(interval_sec);
        double avg_rate = elapsed > 0 ? static_cast<double>(cur) / static_cast<double>(elapsed) : 0.0;

        logger::instr("%02ld:%02ld:%02ld | indexed docs: %llu | docs/sec: %.1f | avg docs/sec: %.1f",
            hrs, mins, secs,
            static_cast<unsigned long long>(cur), rate, avg_rate);
    }
}


deque<string> get_files(uint32_t worker_number) {
    // Fill the file queue: stripe parser files across indexer workers
    deque<string> files;
    for (size_t parser = worker_number; parser < CRAWLER_THREADPOOL_SIZE; parser += NUM_INDEXER_THREADS) {
        string file_name = string::join(
            "",
            string(PARSER_OUTPUT_DIR),
            "/parser_",
            string(parser),
            "_out.txt");

        if (file_exists(file_name)) {
            files.push_back(move(file_name));
            logger::info("Index worker %u found file %s", worker_number, file_name.data());
        }
    }
    return files;
}


void worker(uint32_t worker_number) {
    IndexChunk idx(worker_number);
    deque<string> files = get_files(worker_number);
    size_t initial_files = files.size();
    size_t processed = 0;
    logger::error("Worker %u: starting with %zu files", worker_number, initial_files);
    while (not files.empty()) {
        const string& f = files.front();
        logger::error("Worker %u: file %zu/%zu START: %.*s", worker_number, processed, initial_files, (int)f.size(), f.data());
        bool index_written = idx.index_file(f);
        logger::error("Worker %u: file %zu/%zu DONE  (ok=%d): %.*s", worker_number, processed, initial_files, (int)index_written, (int)f.size(), f.data());
        files.pop_front();
        processed++;
    }
    logger::error("Worker %u: loop done, processed %zu/%zu files, calling final flush", worker_number, processed, initial_files);
    idx.flush();
    logger::error("Worker %u: final flush returned", worker_number);
}


int main(int argc, char* argv[]) {
    logger::error("main: start, NUM_INDEXER_THREADS=%zu", (size_t)NUM_INDEXER_THREADS);
    std::atomic<bool> progress_running{true};
    std::thread reporter(progress_reporter, std::ref(progress_running));

    vector<std::thread> workers;
    for (size_t i = 0; i < NUM_INDEXER_THREADS; i++) {
        workers.push_back(std::thread(worker, i));
        logger::error("main: spawned worker %zu, workers.size()=%zu", i, workers.size());
    }
    logger::error("main: all workers spawned, workers.size()=%zu", workers.size());

    for (size_t i = 0; i < workers.size(); ++i) {
        logger::error("main: about to join worker %zu, joinable=%d", i, (int)workers[i].joinable());
        if (workers[i].joinable()) workers[i].join();
        logger::error("main: joined worker %zu", i);
    }

    progress_running.store(false, std::memory_order_relaxed);
    if (reporter.joinable()) reporter.join();

    logger::instr("indexing complete | total indexed docs: %llu",
        static_cast<unsigned long long>(g_indexed_docs.load(std::memory_order_relaxed)));
    logger::error("main: returning 0");
    return 0;
}