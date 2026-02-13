#include "Index.h"
#include "lib/utils.h"

void init_index() {
    // Find the latest chunk ID
    if (chunk == 0) {
        while (file_exists("index_chunk_" + string::to_string(WORKER_NUMBER) + "_" + string::to_string(chunk) + ".txt")) chunk++;
    }

    IndexChunk index;
}