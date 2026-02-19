#include "../lib/unordered_map.h"
#include "../lib/vector.h"
#include "../lib/string.h"

struct AnchorData {
    uint32_t anchor_id;
    uint32_t freq;
};

struct UserAnchorData {
    const string* anchor_text;
    uint32_t freq;
};

// TODO: decide if vector is better than hashmap for anchor_freqs (cache locality vs. O(1) average lookup)
struct UrlData {
    vector<AnchorData> anchor_freqs;                     // anchor text id to frequency since its last update (potentially size >1)
    uint32_t num_encountered;                            // Number of additional times this URL has been
    uint16_t seed_distance;                              // Distance from seed list (never needs to be updated, so it is not included in the RPC)
    uint16_t eot;                                        // End of title
    uint16_t eod;                                        // End of description
};

class UrlStore {
private:
    unordered_map<string, UrlData> url_data;
    vector<string> anchor_to_id; // anchor text to corresponding id (index)

    const UrlData* findUrlData(const string& url) const;
    UrlData* findUrlData(const string& url);

public:
    UrlStore() {};
    void persist();

    // to read urlStore from disk after a crash, each worker thread will read from its corresponding files and update it's urlstore object accordingly
    static void readFromFile(UrlStore& url_store, const int worker_number);

    bool addUrl(const string& url, const vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t eot, const uint16_t eod, const uint32_t num_encountered);
    bool updateUrl(const string& url, const vector<string>& anchor_texts, const uint32_t num_encountered);

    uint32_t findAnchorId(const string& anchor_text);

    vector<UserAnchorData> getUrlAnchorInfo(const string& url) {
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

    uint32_t getUrlNumEncountered(const string& url) {
        const UrlData* it = findUrlData(url);
        if (it == nullptr) return 0;
        return it->num_encountered;
    }

    uint16_t getUrlSeedDistance(const string& url) {
        const UrlData* it = findUrlData(url);
        return it ? it->seed_distance : UINT16_MAX;
    }

    bool inTitle(const string& url, uint16_t word_pos) {
        const UrlData* it = findUrlData(url);
        return it ? word_pos < it->eot : false;
    }

    bool inDescription(const string& url, uint16_t word_pos) {
        const UrlData* it = findUrlData(url);
        return it ? it->eot <= word_pos && word_pos < it->eod : false;
    }

};

// TODO: This needs to be defined globally on bootup (defining here for now)
const uint32_t WORKER_NUMBER = 0;
