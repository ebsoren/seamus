#include "url_store.h"
#include "../lib/rpc_urlstore.h"
#include "../lib/utils.h"
#include "../lib/algorithm.h"
#include <optional>


UrlStore::UrlStore() {
    rpc_listener = new RPCListener(URL_STORE_PORT, URL_STORE_NUM_THREADS);
    listener_thread = std::thread([this]() {
        rpc_listener->listener_loop([this](int fd) { client_handler(fd); });
    });
}

UrlStore::~UrlStore() {
    rpc_listener->stop();
    if (listener_thread.joinable()) listener_thread.join();
    delete rpc_listener;
}

// Handles a BatchURLStoreUpdateRequest given an ephemeral socket fd  
void UrlStore::client_handler(int fd) {
    std::optional<BatchURLStoreUpdateRequest> req = recv_batch_urlstore_update(fd);
    if (!req) return;

    for (URLStoreUpdateRequest& update_req : req->reqs) {
        if (!updateUrl(update_req.url, update_req.anchor_text, update_req.seed_list_url_hops, update_req.seed_list_domain_hops, update_req.num_encountered)) {
            // if update fails (url DNE), try adding instead
            addUrl(update_req.url, update_req.anchor_text, update_req.seed_list_url_hops, update_req.seed_list_domain_hops, 0, 0, update_req.num_encountered);
        }
    }
}

const UrlData* UrlStore::findUrlData(string& url) const {
    return url_data.get(url);
}

UrlData* UrlStore::findUrlData(string& url) {
    auto slot = url_data.find(url);
    return slot != url_data.end() ? &(*slot).value : nullptr;
}

uint32_t UrlStore::findAnchorId(string& anchor_text) {
    for (size_t i = 0; i < anchor_to_id.size(); i++) {
        if (anchor_to_id[i] == anchor_text) {
            return i;
        }
    }

    // needs explicit copy here to avoid destroying OG anchor text data
    anchor_to_id.push_back(::move(anchor_text));
    return anchor_to_id.size() - 1;
}

void UrlStore::addAnchorFreq(vector<AnchorData>& freqs, uint32_t anchor_id) {
    for (AnchorData& anchor_freq : freqs) {
        if (anchor_freq.anchor_id == anchor_id) {
            anchor_freq.freq++;
            return;
        }
    }

    freqs.push_back({anchor_id, 1});
}


bool UrlStore::addUrl(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint16_t eot, const uint16_t eod, const uint32_t num_encountered) {
    // if url already exists, return false
    if (url_data.find(url) != url_data.end()) return false;

    UrlData new_url_data;
    new_url_data.num_encountered = num_encountered;
    new_url_data.seed_distance = seed_distance;
    new_url_data.domain_dist = domain_distance;
    new_url_data.eot = eot;
    new_url_data.eod = eod;

    for (string& anchor_text : anchor_texts) {
        uint32_t anchor_id = findAnchorId(anchor_text);

        addAnchorFreq(new_url_data.anchor_freqs, anchor_id);
    }

    url_data[url] = new_url_data;
    return true;
}


bool UrlStore::updateUrl(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint32_t num_encountered) {
    UrlData* url_data_ptr = findUrlData(url);
    if (url_data_ptr == nullptr) return false;

    url_data_ptr->num_encountered += num_encountered;
    url_data_ptr->seed_distance = min(url_data_ptr->seed_distance, seed_distance);
    url_data_ptr->domain_dist = min(url_data_ptr->domain_dist, domain_distance);

    for (string& anchor_text : anchor_texts) {
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
            addAnchorFreq(url_data_ptr->anchor_freqs, anchor_id);
        }
    }

    return true;
}


/*
Stored in url_store_<worker #>.txt
Vector of anchor_texts (variable length) seen to id mapping (index)
For each URL:
    <url>\n
    Metadata: <times seen (32 bits)> <distance from seed list (16 bits)> <end of title (16 bits)> <end of description (16 bits)>\n
    Anchor text list.
        For each list: <anchor_text id (32 bits)> <times seen (32 bits)>\n
*/
void UrlStore::persist() {
    string fileName = string::join("urlstore_", string(URL_STORE_WORKER_NUMBER), "_tmp.txt");
    string write_mode("wb");
    FILE* fd = fopen(fileName.data(), write_mode.data());

    if (fd == nullptr) perror("Error opening urlstore file for writing.");

    // Write number of anchors as a binary uint32_t
    uint32_t num_anchors = static_cast<uint32_t>(anchor_to_id.size());
    fwrite(&num_anchors, sizeof(uint32_t), 1, fd);

    for (const string& anchor_text : anchor_to_id) {
        uint32_t anchor_len = static_cast<uint32_t>(anchor_text.size());
        fwrite(&anchor_len, sizeof(uint32_t), 1, fd);
        fwrite(anchor_text.data(), sizeof(char), anchor_len, fd);
    }
    
    for (const auto& slot : url_data) {
        const string& url = slot.key;
        if (url.size() > URL_STORE_MAX_URL_LEN) continue; 
        
        const UrlData& data = slot.value;
        
        uint32_t url_len = static_cast<uint32_t>(url.size());
        fwrite(&url_len, sizeof(uint32_t), 1, fd);
        fwrite(url.data(), sizeof(char), url_len, fd); // FIX: Use .data()

        fwrite(&data.num_encountered, sizeof(uint32_t), 1, fd);
        fwrite(&data.seed_distance, sizeof(uint16_t), 1, fd);
        fwrite(&data.eot, sizeof(uint16_t), 1, fd);
        fwrite(&data.eod, sizeof(uint16_t), 1, fd);

        uint32_t num_freqs = static_cast<uint32_t>(data.anchor_freqs.size());
        fwrite(&num_freqs, sizeof(uint32_t), 1, fd);

        for (const auto& anchor_freq : data.anchor_freqs) {
            fwrite(&anchor_freq.anchor_id, sizeof(uint32_t), 1, fd);
            fwrite(&anchor_freq.freq, sizeof(uint32_t), 1, fd);
        }
    }

    fclose(fd);

    int rc = rename(fileName.data(), string::join("urlstore_", string(URL_STORE_WORKER_NUMBER), ".txt").data());
    if (rc != 0) {
        perror("Error renaming urlstore file");
    }
}

void UrlStore::readFromFile(UrlStore& url_store, const int worker_number) {
    string fileName = string::join("urlstore_", string(worker_number), ".txt");
    string read_mode("rb");
    FILE* fd = fopen(fileName.data(), read_mode.data());

    if (fd == nullptr) {
        perror("Error opening urlstore file for reading.");
        return;
    }

    uint32_t num_anchor_texts;
    if (fread(&num_anchor_texts, sizeof(uint32_t), 1, fd) != 1) {
        fclose(fd);
        return; // handle empty file gracefully
    }

    char anchor_text_buff[URL_STORE_MAX_ANCHOR_TEXT_LEN];
    for (uint32_t i = 0; i < num_anchor_texts; i++) {
        uint32_t anchor_text_len;
        fread(&anchor_text_len, sizeof(uint32_t), 1, fd);
        
        // Guard against file corruption causing buffer overflow
        if (anchor_text_len > URL_STORE_MAX_ANCHOR_TEXT_LEN) anchor_text_len = URL_STORE_MAX_ANCHOR_TEXT_LEN; 
        
        fread(anchor_text_buff, sizeof(char), anchor_text_len, fd);
        url_store.anchor_to_id.push_back(string(anchor_text_buff, anchor_text_len));
    }

    uint32_t url_len;
    char url_buff[URL_STORE_MAX_URL_LEN];
    while (fread(&url_len, sizeof(uint32_t), 1, fd) == 1) {
        if (url_len > URL_STORE_MAX_URL_LEN) url_len = URL_STORE_MAX_URL_LEN;
        fread(url_buff, sizeof(char), url_len, fd);
        
        string url(url_buff, url_len);
        url_store.url_data[url] = UrlData();

        fread(&url_store.url_data[url].num_encountered, sizeof(uint32_t), 1, fd);
        fread(&url_store.url_data[url].seed_distance, sizeof(uint16_t), 1, fd);
        fread(&url_store.url_data[url].eot, sizeof(uint16_t), 1, fd);
        fread(&url_store.url_data[url].eod, sizeof(uint16_t), 1, fd);

        uint32_t num_anchor_freqs;
        fread(&num_anchor_freqs, sizeof(uint32_t), 1, fd);

        for (uint32_t i = 0; i < num_anchor_freqs; i++) {
            uint32_t anchor_id, freq;
            fread(&anchor_id, sizeof(uint32_t), 1, fd);
            fread(&freq, sizeof(uint32_t), 1, fd);
            url_store.url_data[url].anchor_freqs.push_back({anchor_id, freq});
        }
    }

    fclose(fd);
}