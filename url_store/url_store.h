#pragma once

#include "../lib/unordered_map.h"
#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/rpc_listener.h"
#include "../lib/rpc_urlstore.h"
#include "../lib/consts.h"
#include "../crawler/crawler_metrics.h"
#include <cstdint>
#include <functional>
#include <thread>

class DomainCarousel;

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

class Ranker;

class UrlStore {
    friend class Ranker;
private:
    std::mutex global_mtx; // used when reading/modifying data like anchor_to_id
    std::atomic<size_t> unique_url_count{0};
    std::atomic<size_t> total_url_count{0};
    UrlShard shards[URL_NUM_SHARDS];
    unordered_map<string, size_t> anchor_to_id; // anchor text to corresponding id (index)
    vector<string> id_to_anchor;

    DefaultHash<string> hasher;

    std::function<void(size_t, MetricUpdate)> metric_submit;
    std::atomic<uint64_t> local_url_pending{0};
    std::atomic<uint64_t> rpc_url_pending{0};
    std::atomic<uint64_t> local_call_count{0};
    std::atomic<uint64_t> rpc_call_count{0};
    void flush_local_urls();
    void flush_rpc_urls();

    std::atomic<bool> running{true};
    std::mutex shutdown_mtx;
    std::condition_variable shutdown_cv;
    std::thread persist_thread;

    static string lowercase_url(const string& url) {
        char* buf = new char[url.size()];
        for (size_t i = 0; i < url.size(); i++) {
            char c = url.data()[i];
            buf[i] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        }
        string lower(buf, url.size());
        delete[] buf;
        return lower;
    }

    UrlShard& get_shard(const string& url) {
        return shards[hasher(url) % URL_NUM_SHARDS];
    }

    const UrlShard& get_shard(const string& url) const {
        return shards[hasher(url) % URL_NUM_SHARDS];
    }

    UrlShard& get_shard(string& url) {
        return shards[hasher(url) % URL_NUM_SHARDS];
    }


    RPCListener* rpc_listener;      // Listener for client requests
    std::thread listener_thread;    // Thread running the listener loop
    DomainCarousel* dc;             // Frontier carousel for newly discovered URLs
    void client_handler(int fd);    // Detached handler for client requests

    bool addUrl_unlocked(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint32_t num_encountered);
    // to read urlStore from disk after a crash, each worker thread will read from its corresponding files and update it's urlstore object accordingly
    void readFromFile();

    void persist_store_thread() {
        while (running) {
            std::unique_lock<std::mutex> lock(shutdown_mtx);
            shutdown_cv.wait_for(lock, std::chrono::seconds(CRAWLER_PERSIST_INTERVAL_SEC));
            persist();
        }
    }
    
public:
    UrlStore(DomainCarousel* dc, const int worker_num);
    ~UrlStore();

    void set_metric_submit(std::function<void(size_t, MetricUpdate)> fn) { metric_submit = std::move(fn); }

    void record_local_urls(size_t count) {
        local_url_pending.fetch_add(count, std::memory_order_relaxed);
        if (local_call_count.fetch_add(1, std::memory_order_relaxed) % 100 == 99) {
            flush_local_urls();
        }
    }

    void record_rpc_urls(size_t count) {
        rpc_url_pending.fetch_add(count, std::memory_order_relaxed);
        if (rpc_call_count.fetch_add(1, std::memory_order_relaxed) % 100 == 99) {
            flush_rpc_urls();
        }
    }

    void persist(bool final_persist = false);
    bool updateUrl(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint32_t num_encountered, size_t priority = 0);
    void manage_frontier_and_update_url(URLStoreUpdateRequest& req);
    void batch_manage_frontier_and_update_url(BatchURLStoreUpdateRequest& batch_req);
    bool updateTitleLen(const string& url, const uint16_t eot);
    bool updateTitle(const string& url, string& title);
    bool updateBodyLen(const string& url, const uint16_t eod);

    int findAnchorId(string& anchor_text, UrlData* url);

    vector<UrlAnchorData> getUrlAnchorInfo(const string& url) {
        string lurl = lowercase_url(url);
        UrlShard& us = get_shard(lurl);
        std::lock_guard<std::mutex> lock(us.mtx);
        std::lock_guard<std::mutex> lock_global(global_mtx);
        UrlData* it = us.findUrlData(lurl);
        if (it == nullptr) return {};
        if (it->anchor_freqs.size() == 0) return {};

        vector<UrlAnchorData> url_anchor_data;
        url_anchor_data.reserve(it->anchor_freqs.size());
        for (auto anchor_it = it->anchor_freqs.begin(); anchor_it != it->anchor_freqs.end(); ++anchor_it) {
            const auto& tuple = *anchor_it;
            url_anchor_data.push_back({&id_to_anchor[tuple.key] , tuple.value});
        }

        return url_anchor_data;
    }


    bool hasUrl(const string& url) {
        string lurl = lowercase_url(url);
        UrlShard& us = get_shard(lurl);
        std::lock_guard<std::mutex> lock(us.mtx);
        return us.findUrlData(lurl) != nullptr;
    }

    UrlData* getUrl(const string& url) {
        string lurl = lowercase_url(url);
        UrlShard& us = get_shard(lurl);
        std::lock_guard<std::mutex> lock(us.mtx);
        return us.findUrlData(lurl);
    }

    bool hasCrawled(const string& url) {
        string lurl = lowercase_url(url);
        UrlShard& us = get_shard(lurl);
        std::lock_guard<std::mutex> lock(us.mtx);
        const UrlData* data = us.findUrlData(lurl);
        return data && data->crawled;
    }


    void markCrawled(const string& url) {
        string lurl = lowercase_url(url);
        UrlShard& us = get_shard(lurl);
        std::lock_guard<std::mutex> lock(us.mtx);
        UrlData* data = us.findUrlData(lurl);
        if (data) data->crawled = true;
    }


    uint32_t getUrlNumEncountered(const string& url) {
        string lurl = lowercase_url(url);
        UrlShard& us = get_shard(lurl);
        std::lock_guard<std::mutex> lock(us.mtx);
        const UrlData* it = us.findUrlData(lurl);
        if (it == nullptr) return 0;
        return it->num_encountered;
    }


    uint16_t getUrlSeedDistance(const string& url) {
        string lurl = lowercase_url(url);
        UrlShard& us = get_shard(lurl);
        std::lock_guard<std::mutex> lock(us.mtx);
        const UrlData* it = us.findUrlData(lurl);
        return it ? it->seed_distance : UINT16_MAX;
    }


    const string& getTitle(const string& url) {
        string lurl = lowercase_url(url);
        UrlShard& us = get_shard(lurl);
        std::lock_guard<std::mutex> lock(us.mtx);
        const UrlData* it = us.findUrlData(lurl);
        static const string empty("", 0);
        if (!it) return empty;
        return it->title;
    }

    size_t distinct_urls() const { return unique_url_count.load(std::memory_order_relaxed); }
    size_t seen_urls() const { return total_url_count.load(std::memory_order_relaxed); }

    // Memory instrumentation — rough estimate of bytes held by UrlStore structures.
    // Uses capacity() (not size()) so it reflects actual allocated footprint.
    struct MemStats {
        size_t shard_slots_bytes;    // (1 + sizeof(string) + sizeof(UrlData)) × capacity summed across shards
        size_t shard_entry_heap;     // per-entry heap: long-string url/title chars + anchor_freqs mini-map arrays
        size_t anchor_dict_bytes;    // anchor_to_id + id_to_anchor
        size_t total_slots;          // sum of map capacities
        size_t total_entries;        // sum of map sizes
    };

    MemStats mem_stats() {
        MemStats s{};
        for (size_t i = 0; i < URL_NUM_SHARDS; ++i) {
            std::lock_guard<std::mutex> lock(shards[i].mtx);
            auto& m = shards[i].url_data;
            size_t cap = m.capacity();
            s.total_slots += cap;
            s.total_entries += m.size();
            s.shard_slots_bytes += cap * (1 + sizeof(string) + sizeof(UrlData));

            // Per-entry heap charges. This iterates the shard (cost ≈ shard size),
            // so it adds ~ms per shard at 20s drain cadence — acceptable.
            for (auto it = m.begin(); it != m.end(); ++it) {
                auto tup = *it;
                if (tup.key.size() > string::MAX_SHORT_LENGTH) s.shard_entry_heap += tup.key.size() + 1;
                if (tup.value.title.size() > string::MAX_SHORT_LENGTH) s.shard_entry_heap += tup.value.title.size() + 1;
                // anchor_freqs mini-map: 3 arrays (states/keys/values) of capacity() elems
                size_t afc = tup.value.anchor_freqs.capacity();
                s.shard_entry_heap += afc * (1 + sizeof(uint32_t) + sizeof(uint32_t));
            }
        }
        {
            std::lock_guard<std::mutex> lock(global_mtx);
            s.anchor_dict_bytes += anchor_to_id.capacity() * (1 + sizeof(string) + sizeof(size_t));
            for (size_t i = 0; i < id_to_anchor.size(); ++i) {
                s.anchor_dict_bytes += sizeof(string);
                if (id_to_anchor[i].size() > string::MAX_SHORT_LENGTH)
                    s.anchor_dict_bytes += id_to_anchor[i].size() + 1;
            }
        }
        return s;
    }
};


