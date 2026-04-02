#include "url_store.h"
#include "../crawler/domain_carousel.h"
#include "../lib/rpc_urlstore.h"
#include "../lib/utils.h"
#include "../lib/algorithm.h"
#include <optional>


UrlStore::UrlStore(DomainCarousel* dc, const int worker_num) : dc(dc) {
    readFromFile(worker_num);
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

void UrlStore::manage_frontier_and_update_url(URLStoreUpdateRequest& req) {
    bool is_new = updateUrl(req.url, req.anchor_text, req.seed_list_url_hops, req.seed_list_domain_hops, req.num_encountered);

    if (is_new && dc) {
        string domain = extract_domain(req.url);
        CrawlTarget target{
            std::move(domain),
            string(req.url.data(), req.url.size()),
            req.seed_list_url_hops,
            req.seed_list_domain_hops,
        };

        // TODO: Use real priority once frontier scoring is implemented
        size_t priority = 0;
        std::lock_guard<std::mutex> lock(dc->buckets[priority].bucket_lock);
        dc->buckets[priority].urls.push_back(std::move(target));
    }
}

void UrlStore::batch_manage_frontier_and_update_url(BatchURLStoreUpdateRequest& batch_req) {
    // Bucket-sort new URLs by priority, then batch-push to frontier with one lock per bucket
    vector<CrawlTarget> bucket_targets[PRIORITY_BUCKETS];

    for (size_t i = 0; i < batch_req.reqs.size(); ++i) {
        URLStoreUpdateRequest& req = batch_req.reqs[i];
        bool is_new = updateUrl(req.url, req.anchor_text, req.seed_list_url_hops, req.seed_list_domain_hops, req.num_encountered);

        if (is_new && dc) {
            string domain = extract_domain(req.url);
            size_t priority = 0;                                    // TODO: Use real priority once frontier scoring is implemented
            if (priority < PRIORITY_BUCKETS) {
                bucket_targets[priority].push_back(CrawlTarget{
                    std::move(domain),
                    string(req.url.data(), req.url.size()),
                    req.seed_list_url_hops,
                    req.seed_list_domain_hops,
                });
            }
        }
    }

    // One lock acquisition per bucket instead of per URL
    for (size_t p = 0; p < PRIORITY_BUCKETS; ++p) {
        if (bucket_targets[p].size() == 0) continue;
        std::lock_guard<std::mutex> lock(dc->buckets[p].bucket_lock);
        for (size_t i = 0; i < bucket_targets[p].size(); ++i) {
            dc->buckets[p].urls.push_back(std::move(bucket_targets[p][i]));
        }
    }
}

// Handles a BatchURLStoreUpdateRequest given an ephemeral socket fd
void UrlStore::client_handler(int fd) {
    std::optional<BatchURLStoreUpdateRequest> req = recv_batch_urlstore_update(fd);
    if (!req) return;

    batch_manage_frontier_and_update_url(*req);
}


size_t UrlStore::findAnchorId(string& anchor_text) {
    std::lock_guard<std::mutex> lock(global_mtx);
    auto it = anchor_to_id.find(anchor_text);

    if (it == anchor_to_id.end()) {
        anchor_to_id[string(anchor_text.data(), anchor_text.size())] = anchor_to_id.size();
        id_to_anchor.push_back(::move(anchor_text));
        return id_to_anchor.size() - 1;
    }

    return (*it).value;
}

bool UrlStore::addUrl_unlocked(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint32_t num_encountered) {
    UrlShard& us = get_shard(url);
    auto& url_data = us.url_data;

    // if url already exists, return false
    if (url_data.find(url) != url_data.end()) return false;

    url_data[url].num_encountered = num_encountered;
    url_data[url].seed_distance = seed_distance;
    url_data[url].domain_dist = domain_distance;

    for (string& anchor_text : anchor_texts) {
        uint32_t anchor_id = findAnchorId(anchor_text);
        url_data[url].anchor_freqs[anchor_id] = 1;
    }

    return true;
}


// returns whether or not the url was new to the url_store
bool UrlStore::updateUrl(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint32_t num_encountered) {
    UrlShard& us = get_shard(url);
    std::lock_guard<std::mutex> lg(us.mtx);
    UrlData* url_data_ptr = us.findUrlData(url);
    if (url_data_ptr == nullptr) return addUrl_unlocked(url, anchor_texts, seed_distance, domain_distance, num_encountered);

    url_data_ptr->num_encountered += num_encountered;
    url_data_ptr->seed_distance = min(url_data_ptr->seed_distance, seed_distance);
    url_data_ptr->domain_dist = min(url_data_ptr->domain_dist, domain_distance);

    for (string& anchor_text : anchor_texts) {
        uint32_t anchor_id = findAnchorId(anchor_text);
        url_data_ptr->anchor_freqs[anchor_id]++;
    }

    return false;
}

bool UrlStore::updateTitleLen(const string& url, const uint16_t eot) {
    UrlShard& us = get_shard(url);
    std::lock_guard<std::mutex> lg(us.mtx);
    UrlData* url_data_ptr = us.findUrlData(url);
    if (!url_data_ptr) return false;

    url_data_ptr->eot = eot;
    return true;
}

bool UrlStore::updateTitle(const string& url, string& title) {
    UrlShard& us = get_shard(url);
    std::lock_guard<std::mutex> lg(us.mtx);
    UrlData* url_data_ptr = us.findUrlData(url);
    if (!url_data_ptr) return false;

    url_data_ptr->title = ::move(title);
    return true;
}


bool UrlStore::updateBodyLen(const string& url, const uint16_t eod) {
    UrlShard& us = get_shard(url);
    std::lock_guard<std::mutex> lg(us.mtx);
    UrlData* url_data_ptr = us.findUrlData(url);
    if (!url_data_ptr) return false;

    url_data_ptr->eod = eod;
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
    vector<string> anchor_snapshot;

    {
        std::lock_guard<std::mutex> lock(global_mtx);
        anchor_snapshot.reserve(id_to_anchor.size());
        
        for (const string& anchor_text : id_to_anchor) {
            anchor_snapshot.push_back(string(anchor_text.data(), anchor_text.size()));
        }
    }
    // Write number of anchors as a binary uint32_t
    uint32_t num_anchors = static_cast<uint32_t>(anchor_snapshot.size());
    fwrite(&num_anchors, sizeof(uint32_t), 1, fd);

    for (const string& anchor_text : anchor_snapshot) {
        uint32_t anchor_len = static_cast<uint32_t>(anchor_text.size());
        fwrite(&anchor_len, sizeof(uint32_t), 1, fd);
        fwrite(anchor_text.data(), sizeof(char), anchor_len, fd);
    }
    
    for (size_t i = 0; i < URL_NUM_SHARDS; i++) {
        auto& curr_shard = *(shards + i);
        std::lock_guard<std::mutex> lock(curr_shard.mtx);
        auto& url_data = curr_shard.url_data;
        for (const auto& slot : url_data) {
            const string& url = slot.key;
            if (url.size() > URL_STORE_MAX_URL_LEN) continue; 
            
            UrlData& data = slot.value;
            
            uint32_t url_len = static_cast<uint32_t>(url.size());
            fwrite(&url_len, sizeof(uint32_t), 1, fd);
            fwrite(url.data(), sizeof(char), url_len, fd); // FIX: Use .data()

            fwrite(&data.num_encountered, sizeof(uint32_t), 1, fd);
            fwrite(&data.seed_distance, sizeof(uint16_t), 1, fd);
            fwrite(&data.eot, sizeof(uint16_t), 1, fd);

            size_t title_len = data.title.size();
            fwrite(&title_len, sizeof(size_t), 1, fd);
            fwrite(data.title.data(), sizeof(char), title_len, fd);

            fwrite(&data.eod, sizeof(uint16_t), 1, fd);

            uint32_t num_freqs = static_cast<uint32_t>(data.anchor_freqs.size());
            fwrite(&num_freqs, sizeof(uint32_t), 1, fd);

            for (auto it = data.anchor_freqs.begin(); it != data.anchor_freqs.end(); ++it) {
                const auto& tuple = *it;
                fwrite(&tuple.key, sizeof(uint32_t), 1, fd);
                fwrite(&tuple.value, sizeof(uint32_t), 1, fd);
            }
        }
    }

    fclose(fd);

    int rc = rename(fileName.data(), string::join("urlstore_", string(URL_STORE_WORKER_NUMBER), ".txt").data());
    if (rc != 0) {
        perror("Error renaming urlstore file");
    }
}

void UrlStore::readFromFile(const int worker_number) {
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
        id_to_anchor.push_back(string(anchor_text_buff, anchor_text_len));
        anchor_to_id[string(anchor_text_buff, anchor_text_len)] = id_to_anchor.size() - 1;
    }

    uint32_t url_len;
    char url_buff[URL_STORE_MAX_URL_LEN];
    while (fread(&url_len, sizeof(uint32_t), 1, fd) == 1) {
        if (url_len > URL_STORE_MAX_URL_LEN) url_len = URL_STORE_MAX_URL_LEN;
        fread(url_buff, sizeof(char), url_len, fd);
        
        string url(url_buff, url_len);

        // TODO: is locking needed here? supposedly readFromFile should only be called on startup before requests are sent right
        UrlShard& shard = get_shard(url);
        // std::lock_guard<std::mutex> lock(shard.mtx);
        auto& url_data = shard.url_data;

        fread(&url_data[url].num_encountered, sizeof(uint32_t), 1, fd);
        fread(&url_data[url].seed_distance, sizeof(uint16_t), 1, fd);
        fread(&url_data[url].eot, sizeof(uint16_t), 1, fd);

        size_t title_len;
        fread(&title_len, sizeof(size_t), 1, fd);
        char title_buf[title_len];
        fread(title_buf, sizeof(char), title_len, fd);
        url_data[url].title = string(title_buf, title_len);
        
        fread(&url_data[url].eod, sizeof(uint16_t), 1, fd);

        uint32_t num_anchor_freqs;
        fread(&num_anchor_freqs, sizeof(uint32_t), 1, fd);

        for (uint32_t i = 0; i < num_anchor_freqs; i++) {
            uint32_t anchor_id, freq;
            fread(&anchor_id, sizeof(uint32_t), 1, fd);
            fread(&freq, sizeof(uint32_t), 1, fd);
            url_data[url].anchor_freqs[anchor_id] = freq;
        }
    }

    fclose(fd);
}
