#include "../lib/unordered_map.h"
#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/rpc_listener.h"
#include "../lib/rpc_urlstore.h"
#include "../lib/consts.h"
#include <cstdint>
#include <thread>

#include <iostream>


struct UrlAnchorData {
    string* anchor_text;
    uint32_t freq;
};

struct UrlShard {
    mutable std::mutex mtx;
    unordered_map<string, UrlData> url_data;

    const UrlData* findUrlData(const string& url) const {
        return url_data.get(url);
    }

    UrlData* findUrlData(const string& url) {
        auto slot = url_data.find(url);
        return slot != url_data.end() ? &(*slot).value : nullptr;
    }
};

class UrlStore {
private:
    std::mutex global_mtx; // used when reading/modifying data like anchor_to_id
    UrlShard shards[URL_NUM_SHARDS];
    vector<string> anchor_to_id; // anchor text to corresponding id (index)

    DefaultHash<string> hasher;
    UrlShard& get_shard(const string& url) {
        return shards[hasher(url) % URL_NUM_SHARDS];
    }

    const UrlShard& get_shard(const string& url) const {
        return shards[hasher(url) % URL_NUM_SHARDS];
    }

    UrlShard& get_shard(string& url) {
        return shards[hasher(url) % URL_NUM_SHARDS];
    }

    const UrlData* findUrlData(const string& url) const;
    UrlData* findUrlData(const string& url);

    RPCListener* rpc_listener;      // Listener for client requests
    std::thread listener_thread;    // Thread running the listener loop
    void client_handler(int fd);    // Detached handler for client requests

    bool addUrl_unlocked(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint32_t num_encountered);
    
public:
    UrlStore();
    ~UrlStore();
    void persist();

    // to read urlStore from disk after a crash, each worker thread will read from its corresponding files and update it's urlstore object accordingly
    void readFromFile(const int worker_number);

    // bool addUrl(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint16_t eot, const uint16_t eod, const uint32_t num_encountered);
    bool updateUrl(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint32_t num_encountered);
    bool updateTitleLen(const string& url, const uint16_t eot);
    bool updateBodyLen(const string& url, const uint16_t eod);

    uint32_t findAnchorId(string& anchor_text);

    vector<UrlAnchorData> getUrlAnchorInfo(const string& url) {
        UrlShard& us = get_shard(url);
        std::lock_guard<std::mutex> lock(us.mtx);
        std::lock_guard<std::mutex> lock_global(global_mtx);
        UrlData* it = us.findUrlData(url);
        if (it == nullptr) return {};
        if (it->anchor_freqs.size() == 0) return {};

        vector<UrlAnchorData> url_anchor_data;
        url_anchor_data.reserve(it->anchor_freqs.size());
        for (auto anchor_it = it->anchor_freqs.begin(); anchor_it != it->anchor_freqs.end(); ++anchor_it) {
            const auto& tuple = *anchor_it;
            url_anchor_data.push_back({&anchor_to_id[tuple.key] , tuple.value});
        }

        return url_anchor_data;
    }


    uint32_t getUrlNumEncountered(const string& url) {
        UrlShard& us = get_shard(url);
        std::lock_guard<std::mutex> lock(us.mtx);
        const UrlData* it = us.findUrlData(url);
        if (it == nullptr) return 0;
        return it->num_encountered;
    }


    uint16_t getUrlSeedDistance(const string& url) {
        UrlShard& us = get_shard(url);
        std::lock_guard<std::mutex> lock(us.mtx);
        const UrlData* it = us.findUrlData(url);
        return it ? it->seed_distance : UINT16_MAX;
    }


    bool inTitle(const string& url, uint16_t word_pos) {
        UrlShard& us = get_shard(url);
        std::lock_guard<std::mutex> lock(us.mtx);
        const UrlData* it = us.findUrlData(url);
        return it ? word_pos < it->eot : false;
    }


    bool inDescription(const string& url, uint16_t word_pos) {
        UrlShard& us = get_shard(url);
        std::lock_guard<std::mutex> lock(us.mtx);
        const UrlData* it = us.findUrlData(url);
        return it ? it->eot <= word_pos && word_pos < it->eod : false;
    }
};


