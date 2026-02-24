#include "url_store.h"
#include <optional>


UrlStore::UrlStore() {
    rpc_listener = new RPCListener(PORT, NUM_THREADS);
    // TODO(hershey): spawn listener loop in a detached thread here
}

UrlStore::~UrlStore() {
    delete rpc_listener;
}

void UrlStore::client_handler(int fd) {
    std::optional<URLStoreUpdateRequest> req = recv_urlstore_update(fd);
    // TODO(hershey): finish the RPC wrapper logic
}

const UrlData* UrlStore::findUrlData(const string& url) const {
    const Slot<string, UrlData>* slot = url_data.find(url);
    return slot ? &slot->value : nullptr;
}

UrlData* UrlStore::findUrlData(const string& url) {
    Slot<string, UrlData>* slot = url_data.find(url);
    return slot ? &slot->value : nullptr;
}

uint32_t UrlStore::findAnchorId(const string& anchor_text) {
    for (size_t i = 0; i < anchor_to_id.size(); i++) {
        if (anchor_to_id[i] == anchor_text) {
            return i;
        }
    }
    anchor_to_id.push_back(anchor_text);
    return anchor_to_id.size() - 1;
}


bool UrlStore::addUrl(const string& url, const vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t eot, const uint16_t eod, const uint32_t num_encountered) {
    // if url already exists, return false
    if (url_data.find(url)) return false;

    UrlData new_url_data;
    new_url_data.num_encountered = num_encountered;
    new_url_data.seed_distance = seed_distance;
    new_url_data.eot = eot;
    new_url_data.eod = eod;

    for (const string& anchor_text : anchor_texts) {
        uint32_t anchor_id = findAnchorId(anchor_text);
        // TODO: revisit if anchor_texts content can be duplicates
        new_url_data.anchor_freqs.push_back({anchor_id, 1});
    }

    url_data[url] = new_url_data;
    return true;
}


bool UrlStore::updateUrl(const string& url, const vector<string>& anchor_texts, const uint32_t num_encountered) {
    UrlData* url_data_ptr = findUrlData(url);
    if (url_data_ptr == nullptr) return false;

    url_data_ptr->num_encountered += num_encountered;

    for (const string& anchor_text : anchor_texts) {
        uint32_t anchor_id = findAnchorId(anchor_text);

        bool found_anchor_freq = false;
        for (AnchorData& anchor_freq : url_data_ptr->anchor_freqs) {
            if (anchor_freq.anchor_id == anchor_id) {
                anchor_freq.freq++;
                found_anchor_freq = true;
                break;
            }
        }

        if (!found_anchor_freq) {
            url_data_ptr->anchor_freqs.push_back({anchor_id, 1});
        }
    }

    return true;
}


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
    string fileName = string::join("urlstore_", string(WORKER_NUMBER), ".txt");
    FILE* fd = fopen(fileName.data(), "wx");

    if (fd == nullptr) perror("Error opening urlstore file for writing.");

    fprintf(fd, "%u", anchor_to_id.size());
    for (const string& anchor_text : anchor_to_id) {
        fprintf(fd, "%lu", anchor_text.size());
        if (anchor_text.size() > MAX_ANCHOR_TEXT_LEN) {
            fprintf(stderr, "Warning: Anchor text '%s' exceeds max length and will be truncated.\n", anchor_text.data());
        }
        fwrite(&anchor_text, sizeof(char), anchor_text.size(), fd);
        fwrite("\n", sizeof(char), 1, fd);
    }
    
    for (const auto& slot : url_data) {
        const string& url = slot.key;
        if (url.size() > MAX_URL_LEN) continue; // skip urls that exceed max length
        const UrlData& data = slot.value;
        fprintf(fd, "%lu", url.size());
        fwrite(&url, sizeof(char), url.size(), fd);
        fwrite("\n", sizeof(char), 1, fd);
        fprintf(fd, "%u %u %u %u %u\n", data.num_encountered, data.seed_distance, data.eot, data.eod, data.anchor_freqs.size());

        for (const auto& anchor_freq : data.anchor_freqs) {
            fprintf(fd, "%u %u\n", anchor_freq.anchor_id, anchor_freq.freq);
        }
    }

    fclose(fd);
}

void UrlStore::readFromFile(UrlStore& url_store, const int worker_number) {
    // given a urlstore object and worker number, read from corresponding file and update urlstore object accordingly
    string fileName = string::join("urlstore_", string(worker_number), ".txt");
    FILE* fd = fopen(fileName.data(), "r");

    if (fd == nullptr) perror("Error opening urlstore file for reading.");

    uint32_t num_anchor_texts;
    char dummy;
    fread(&num_anchor_texts, sizeof(uint32_t), 1, fd);
    char anchor_text_buff[MAX_ANCHOR_TEXT_LEN];
    for (uint32_t i = 0; i < num_anchor_texts; i++) {
        uint32_t anchor_text_len;
        fread(&anchor_text_len, sizeof(uint32_t), 1, fd);
        fread(&anchor_text_buff, sizeof(char), anchor_text_len, fd);
        string anchor_text(anchor_text_buff, anchor_text_len);

        url_store.anchor_to_id.push_back(anchor_text);
        fread(&dummy, sizeof(char), 1, fd); // consume newline
    }

    uint32_t url_len;
    char url_buff[MAX_URL_LEN];
    while (fread(&url_len, sizeof(uint32_t), 1, fd)) {
        fread(&url_buff, sizeof(char), url_len, fd);
        string url(url_buff, url_len);
        fread(&dummy, sizeof(char), 1, fd); // consume newline
        url_store.url_data[url] = UrlData();

        uint32_t num_encountered, num_anchor_freqs;
        uint16_t seed_distance, eot, eod;
        fread(&num_encountered, sizeof(uint32_t), 1, fd);
        fread(&seed_distance, sizeof(uint16_t), 1, fd);
        fread(&eot, sizeof(uint16_t), 1, fd);
        fread(&eod, sizeof(uint16_t), 1, fd);
        fread(&num_anchor_freqs, sizeof(uint32_t), 1, fd);
        url_store.url_data[url].num_encountered = num_encountered;
        url_store.url_data[url].seed_distance = seed_distance;
        url_store.url_data[url].eot = eot;
        url_store.url_data[url].eod = eod;

        fread(&dummy, sizeof(char), 1, fd); // consume newline

        for (uint32_t i = 0; i < num_anchor_freqs; i++) {
            uint32_t anchor_id, freq;
            fread(&anchor_id, sizeof(uint32_t), 1, fd);
            fread(&freq, sizeof(uint32_t), 1, fd);
            fread(&dummy, sizeof(char), 1, fd); // consume newline

            url_store.url_data[url].anchor_freqs.push_back({anchor_id, freq});
        }
    }

    fclose(fd);
}
