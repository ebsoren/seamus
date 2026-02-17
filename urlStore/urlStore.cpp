#include "urlStore.h"

/*
Persisted at same time as index chunks

Stored in url_store_<worker #>.txt
Vector of anchor_texts (variable length) seen to id mapping (index)
For each URL:
    <url>\n
    Metadata: <times seen (32 bits)> <distance from seed list (16 bits)> <end of title (16 bits)> <end of description (16 bits)>\n
    Anchor text list.
        For each list: <anchor_text id (32 bits)> <times seen (32 bits)>\n
*/
void UrlStore::persist() {
    // given the current data and worker this urlstore is assigned to
    // persist to provided file
    FILE* fd = fopen("urlstore_" + string::to_string(WORKER_NUMBER) + ".txt", "wx");

    if (fd == nullptr) perror("Error opening urlstore file for writing.");

    for (const string& anchor_text : anchor_to_id) {
        fwrite(&anchor_text, sizeof(char), anchor_text.size(), fd);
        fwrite("\n", sizeof(char), 1, fd);
    }
    
    for (const auto& [url, data] : url_data) {
        fwrite(&url, sizeof(char), url.size(), fd);
        fprintf(fd, "%u %u %u %u\n", data.num_encountered, data.seed_distance, data.eot, data.eod);
        for (const auto& anchor_freq : data.anchor_freqs) {
            fprintf(fd, "%u %u\n", anchor_freq.anchor_id, anchor_freq.freq);
        }
    }
}