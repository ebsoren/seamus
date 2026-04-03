#include "Index.h"
#include <thread>
#include "lib/logger.h"


deque<string> get_files(uint32_t worker_number) {
    // Fill the file queue
    deque<string> files;
    uint32_t i = 0;
    while (file_exists(string::join("parsed_docs_", string(NUM_CORES*i+worker_number), ".txt"))) {
        files.push_back(string::join("parsed_docs_", string(NUM_CORES*(i++)+worker_number), ".txt"));
    }
    return files;
}


void worker(uint32_t worker_number) {
    IndexChunk idx(worker_number);
    deque<string> files = get_files(worker_number);
    while (not files.empty()) {
        bool index_written = idx.index_file(files.front());
        files.pop_front();
    }
    idx.flush();
    logger::info("Index worker %u completed", worker_number);
}


int main(int argc, char* argv[]) {

    if (argc != 2) {
        perror("Usage: ./index <worker number>");
        exit(1);
    }


    vector<std::thread> workers;
    for (size_t i = 0; i < NUM_CORES; i++) {
        workers.push_back(std::thread(worker, i));
    }

    for (size_t i = 0; i < workers.size(); ++i) {
        if (workers[i].joinable()) workers[i].join();
    }
    return 0;
}