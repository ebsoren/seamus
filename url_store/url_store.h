#include "../lib/unordered_map.h"
#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/rpc_listener.h"
#include "../lib/rpc_urlstore.h"
#include "../lib/consts.h"
#include <cstdint>
#include <thread>

#include <iostream>


struct UserAnchorData {
    string* anchor_text;
    uint32_t freq;
};

class UrlStore {
private:
    unordered_map<string, UrlData> url_data;
    vector<string> anchor_to_id; // anchor text to corresponding id (index)

    const UrlData* findUrlData(string& url) const;
    UrlData* findUrlData(string& url);
    void addAnchorFreq(vector<AnchorData>& freqs, uint32_t anchor_id);

    RPCListener* rpc_listener;      // Listener for client requests
    std::thread listener_thread;    // Thread running the listener loop
    void client_handler(int fd);    // Detached handler for client requests


public:
    UrlStore();
    ~UrlStore();
    void persist();

    // to read urlStore from disk after a crash, each worker thread will read from its corresponding files and update it's urlstore object accordingly
    static void readFromFile(UrlStore& url_store, const int worker_number);

    bool addUrl(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint16_t eot, const uint16_t eod, const uint32_t num_encountered);
    bool updateUrl(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint32_t num_encountered);

    uint32_t findAnchorId(string& anchor_text);

    vector<UserAnchorData> getUrlAnchorInfo(string& url) {
        const UrlData* it = findUrlData(url);
        if (it == nullptr) return {};
        if (it->anchor_freqs.empty()) return {};

        vector<UserAnchorData> user_anchor_data;
        user_anchor_data.reserve(it->anchor_freqs.size());
        for (const AnchorData& anchor_freq : it->anchor_freqs) {
            user_anchor_data.push_back({&anchor_to_id[anchor_freq.anchor_id] , anchor_freq.freq});
        }

        return user_anchor_data;
    }


    uint32_t getUrlNumEncountered(string& url) {
        const UrlData* it = findUrlData(url);
        if (it == nullptr) return 0;
        return it->num_encountered;
    }


    uint16_t getUrlSeedDistance(string& url) {
        const UrlData* it = findUrlData(url);
        return it ? it->seed_distance : UINT16_MAX;
    }


    bool inTitle(string& url, uint16_t word_pos) {
        const UrlData* it = findUrlData(url);
        return it ? word_pos < it->eot : false;
    }


    bool inDescription(string& url, uint16_t word_pos) {
        const UrlData* it = findUrlData(url);
        return it ? it->eot <= word_pos && word_pos < it->eod : false;
    }
};


