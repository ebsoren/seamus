#include "Index.h"
#include <thread>
#include "lib/logger.h"


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
    logger::error("main: returning 0");
    return 0;
}