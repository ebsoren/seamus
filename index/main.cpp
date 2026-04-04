#include "Index.h"
#include <thread>
#include "lib/logger.h"


deque<string> get_files(uint32_t worker_number) {
    // Fill the file queue
    deque<string> files;
    size_t i = 0;
    while (true) {
        bool file_found = false;
        for (size_t parser = worker_number; parser < CRAWLER_THREADPOOL_SIZE; parser+=NUM_INDEXER_THREADS) {
            string file_name = string::join(
                "",
                string(PARSER_OUTPUT_DIR),
                "/parser_",
                string(i),
                "_out_",
                string(parser),
                ".txt");

            if (file_exists(file_name)) {
                files.push_back(move(file_name));
                file_found = true;
                logger::info("Index worker %u found file %s", worker_number, file_name.data());
            }
        }
        if (not file_found) {
            return files;
        };
        i++;
    }
    return files;
}


void worker(uint32_t worker_number) {
    IndexChunk idx(worker_number);
    deque<string> files = get_files(worker_number);
    while (not files.empty()) {
        bool index_written = idx.index_file(files.front()); // TODO do something if false?
        files.pop_front();
    }
    idx.flush();
    logger::info("Index worker %u completed", worker_number);
}


int main(int argc, char* argv[]) {
    vector<std::thread> workers;
    for (size_t i = 0; i < NUM_INDEXER_THREADS; i++) {
        workers.push_back(std::thread(worker, i));
    }

    for (size_t i = 0; i < workers.size(); ++i) {
        if (workers[i].joinable()) workers[i].join();
    }
    return 0;
}