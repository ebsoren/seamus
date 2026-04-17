#include "url_store.h"
#include "../crawler/domain_carousel.h"
#include "../lib/rpc_urlstore.h"
#include "../lib/utils.h"
#include "../lib/algorithm.h"
#include "../lib/Frontier.h"
#include "../lib/logger.h"
#include "../lib/url_filter.h"
#include <optional>


UrlStore::UrlStore(DomainCarousel* dc, const int worker_num) : dc(dc) {
    if (!URL_FROM_SCRATCH) readFromFile();
    
    rpc_listener = new RPCListener(URL_STORE_PORT, URL_STORE_NUM_THREADS);
    listener_thread = std::thread([this]() {
        rpc_listener->listener_loop([this](int fd) { client_handler(fd); });
    });

    mkdir(URL_STORE_OUTPUT_DIR, 0755);   // no-op if already exists
    persist_thread = std::thread(&UrlStore::persist_store_thread, this);
}

UrlStore::~UrlStore() {
    running.store(false, std::memory_order_relaxed);
    shutdown_cv.notify_all();
    rpc_listener->stop();
    if (listener_thread.joinable()) listener_thread.join();
    if (persist_thread.joinable()) persist_thread.join();
    delete rpc_listener;

    persist(true); // only persist dirty urls (parsed)
}

void UrlStore::flush_local_urls() {
    if (!metric_submit) return;
    uint64_t pending = local_url_pending.exchange(0, std::memory_order_relaxed);
    if (pending > 0) {
        metric_submit(0, MetricUpdate{MetricType::LOCAL_URL_ACCUMULATE, static_cast<double>(pending), 0});
    }
}

void UrlStore::flush_rpc_urls() {
    if (!metric_submit) return;
    uint64_t pending = rpc_url_pending.exchange(0, std::memory_order_relaxed);
    if (pending > 0) {
        metric_submit(0, MetricUpdate{MetricType::RECEIVED_URL_ACCUMULATE, static_cast<double>(pending), 0});
    }
}

void UrlStore::manage_frontier_and_update_url(URLStoreUpdateRequest& req) {
    size_t priority = get_priority_bucket(req.url, req.seed_list_url_hops, req.seed_list_domain_hops);
    bool is_new = updateUrl(req.url, req.anchor_text, req.seed_list_url_hops, req.seed_list_domain_hops, req.num_encountered, priority);

    if (is_new && dc && priority < PRIORITY_BUCKETS) {
        string domain = extract_domain(req.url);
        CrawlTarget target{
            std::move(domain),
            string(req.url.data(), req.url.size()),
            req.seed_list_url_hops,
            req.seed_list_domain_hops,
        };

        std::lock_guard<std::mutex> lock(dc->buckets[priority].bucket_lock);
        dc->buckets[priority].urls.push_back(std::move(target));
    }
}

void UrlStore::batch_manage_frontier_and_update_url(BatchURLStoreUpdateRequest& batch_req) {
    // Bucket-sort new URLs by priority, then batch-push to frontier with one lock per bucket
    vector<CrawlTarget> bucket_targets[PRIORITY_BUCKETS];

    for (size_t i = 0; i < batch_req.reqs.size(); ++i) {
        URLStoreUpdateRequest& req = batch_req.reqs[i];
        size_t priority = get_priority_bucket(req.url, req.seed_list_url_hops, req.seed_list_domain_hops);
        bool is_new = updateUrl(req.url, req.anchor_text, req.seed_list_url_hops, req.seed_list_domain_hops, req.num_encountered, priority);

        if (is_new && dc && priority < PRIORITY_BUCKETS) {
            string domain = extract_domain(req.url);
            bucket_targets[priority].push_back(CrawlTarget{
                std::move(domain),
                string(req.url.data(), req.url.size()),
                req.seed_list_url_hops,
                req.seed_list_domain_hops,
            });
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
    close(fd);
    if (!req) return;

    record_rpc_urls(req->reqs.size());
    batch_manage_frontier_and_update_url(*req);
}


int UrlStore::findAnchorId(string& anchor_text, UrlData* url_data) {
    std::lock_guard<std::mutex> lock(global_mtx);
    auto it = anchor_to_id.find(anchor_text);

    if (it == anchor_to_id.end()) {
        if (url_data && url_data->anchor_freqs.size() >= MAX_ANCHORS_PER_URL) return -1;
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

    unique_url_count.fetch_add(1, std::memory_order_relaxed);
    url_data[url].num_encountered = num_encountered;
    url_data[url].seed_distance = seed_distance;
    url_data[url].domain_dist = domain_distance;

    for (string& anchor_text : anchor_texts) {
        int anchor_id = findAnchorId(anchor_text, us.findUrlData(url));
        if (anchor_id == -1) continue; // skip anchor text if url already has max anchors
        url_data[url].anchor_freqs[anchor_id] = 1;
    }

    return true;
}


// returns whether or not the url was new to the url_store
bool UrlStore::updateUrl(string& url, vector<string>& anchor_texts, const uint16_t seed_distance, const uint16_t domain_distance, const uint32_t num_encountered, size_t priority) {
    total_url_count.fetch_add(1, std::memory_order_relaxed);
    UrlShard& us = get_shard(url);
    std::lock_guard<std::mutex> lg(us.mtx);
    UrlData* url_data_ptr = us.findUrlData(url);
    if (url_data_ptr == nullptr) {
        // If URL priority is low enough that link will never be crawled, don't
        // add to URLStore
        if (priority >= PRIORITY_BUCKETS) {
            logger::debug("updateUrl: dropping url due to low priority (priority=%zu): %s", priority, url.data());
            return false;
        }
        if (is_nsfw_url(url)) {
            logger::debug("updateUrl: dropping nsfw url: %s", url.data());
            return false;
        }
        if (unique_url_count.load(std::memory_order_relaxed) >= MAX_STORE_URLS) {
            logger::warn("UrlStore has reached max capacity, not adding new URL: %s", url.data());
            return false;
        }
        return addUrl_unlocked(url, anchor_texts, seed_distance, domain_distance, num_encountered);
    }

    url_data_ptr->num_encountered += num_encountered;
    url_data_ptr->seed_distance = min(url_data_ptr->seed_distance, seed_distance);
    url_data_ptr->domain_dist = min(url_data_ptr->domain_dist, domain_distance);

    for (string& anchor_text : anchor_texts) {
        int anchor_id = findAnchorId(anchor_text, url_data_ptr);
        if (anchor_id == -1) continue; // skip anchor text if url already has max anchors
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
void UrlStore::persist(bool final_persist) {
    logger::info("Persisting UrlStore to disk...");
    string fileName = string::join("", URL_STORE_OUTPUT_DIR_STR, "/urlstore_tmp.txt");
    string write_mode("wb");
    FILE* fd = fopen(fileName.data(), write_mode.data());

    if (fd == nullptr) { perror("Error opening urlstore file for writing."); return; }
    vector<string> anchor_snapshot;

    {
        std::lock_guard<std::mutex> lock(global_mtx);
        // maybe check here if size constraint is met to persist, and exit early if not?
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
            if (final_persist && !slot.value.crawled) continue; // if final persist, only persist urls that have been crawled (parsed)
            
            UrlData& data = slot.value;
            
            uint32_t url_len = static_cast<uint32_t>(url.size());
            fwrite(&url_len, sizeof(uint32_t), 1, fd);
            fwrite(url.data(), sizeof(char), url_len, fd);

            fwrite(&data.num_encountered, sizeof(uint32_t), 1, fd);
            fwrite(&data.seed_distance, sizeof(uint16_t), 1, fd);
            fwrite(&data.eot, sizeof(uint16_t), 1, fd);

            size_t title_len = data.title.size();
            fwrite(&title_len, sizeof(size_t), 1, fd);
            fwrite(data.title.data(), sizeof(char), title_len, fd);

            fwrite(&data.eod, sizeof(uint16_t), 1, fd);

            fwrite(&data.domain_dist, sizeof(uint16_t), 1, fd);
            fwrite(&data.crawled, sizeof(bool), 1, fd);

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

    string final_file = string::join("", URL_STORE_OUTPUT_DIR_STR, "/urlstore.txt");
    int rc = rename(fileName.data(), final_file.data());
    if (rc != 0) {
        perror("Error renaming urlstore file");
    }
}

void UrlStore::readFromFile() {
    // 1. FIX: Ensure each worker reads its own specific file to avoid data races
    // Note: Make sure your custom string::join supports this, or use std::to_string
    string fileName = string::join("", URL_STORE_OUTPUT_DIR_STR, "/urlstore.txt");
    
    string read_mode("rb");
    FILE* fd = fopen(fileName.data(), read_mode.data());

    if (fd == nullptr) {
        logger::error("url_store: FAILED to open %s for reading", fileName.data());
        return;
    }

    uint32_t num_anchor_texts;
    if (fread(&num_anchor_texts, sizeof(uint32_t), 1, fd) != 1) {
        fclose(fd);
        return; // handle empty file gracefully
    }
    fprintf(stderr, "[URL_STORE] reading %u anchor texts, file pos after header: %ld\n",
            num_anchor_texts, ftell(fd));

    size_t oversized_anchors = 0;
    size_t expected_bytes = 0;
    long pos_before_anchors = ftell(fd);
    char anchor_text_buff[URL_STORE_MAX_ANCHOR_TEXT_LEN];
    for (uint32_t i = 0; i < num_anchor_texts; i++) {
        uint32_t anchor_text_len;
        size_t r = fread(&anchor_text_len, sizeof(uint32_t), 1, fd);
        if (r != 1) {
            fprintf(stderr, "[URL_STORE] anchor[%u]: fread of len failed (r=%zu), pos=%ld\n", i, r, ftell(fd));
            break;
        }
        expected_bytes += 4 + anchor_text_len;

        // Guard against file corruption causing buffer overflow
        if (anchor_text_len > URL_STORE_MAX_ANCHOR_TEXT_LEN) {
            oversized_anchors++;
            fread(anchor_text_buff, sizeof(char), URL_STORE_MAX_ANCHOR_TEXT_LEN, fd);
            fseek(fd, anchor_text_len - URL_STORE_MAX_ANCHOR_TEXT_LEN, SEEK_CUR);
            anchor_text_len = URL_STORE_MAX_ANCHOR_TEXT_LEN;
        } else {
            size_t rd = fread(anchor_text_buff, sizeof(char), anchor_text_len, fd);
            if (rd != anchor_text_len) {
                fprintf(stderr, "[URL_STORE] anchor[%u]: short read! expected %u got %zu, pos=%ld\n",
                        i, anchor_text_len, rd, ftell(fd));
            }
        }
        id_to_anchor.push_back(string(anchor_text_buff, anchor_text_len));
        anchor_to_id[string(anchor_text_buff, anchor_text_len)] = id_to_anchor.size() - 1;
    }
    long pos_after_anchors = ftell(fd);
    long actual_bytes = pos_after_anchors - pos_before_anchors;
    fprintf(stderr, "[URL_STORE] anchor byte accounting: expected=%zu actual=%ld delta=%ld\n",
            expected_bytes, actual_bytes, actual_bytes - (long)expected_bytes);
    // Log the last few anchors to see if they look sane at the boundary
    if (id_to_anchor.size() >= 2) {
        const string& last = id_to_anchor[id_to_anchor.size() - 1];
        const string& second_last = id_to_anchor[id_to_anchor.size() - 2];
        fprintf(stderr, "[URL_STORE] last anchor[%zu] len=%zu: '%.*s'\n",
                id_to_anchor.size()-1, last.size(),
                static_cast<int>(last.size() > 80 ? 80 : last.size()), last.data());
        fprintf(stderr, "[URL_STORE] anchor[%zu] len=%zu: '%.*s'\n",
                id_to_anchor.size()-2, second_last.size(),
                static_cast<int>(second_last.size() > 80 ? 80 : second_last.size()), second_last.data());
    }

    // Peek at raw bytes at the boundary to see what's actually there
    long boundary_pos = ftell(fd);
    unsigned char peek[16];
    size_t peeked = fread(peek, 1, 16, fd);
    fseek(fd, boundary_pos, SEEK_SET);  // seek back
    fprintf(stderr, "[URL_STORE] anchors done, oversized=%zu, file pos: %ld, next 16 bytes: ",
            oversized_anchors, boundary_pos);
    for (size_t pi = 0; pi < peeked; pi++) fprintf(stderr, "%02x ", peek[pi]);
    fprintf(stderr, "\n");
    fflush(stderr);

    size_t url_len;
    char url_buff[URL_STORE_MAX_URL_LEN];
    size_t url_count = 0;
    while (fread(&url_len, sizeof(uint32_t), 1, fd) == 1) {
        if (url_len > URL_STORE_MAX_URL_LEN) {
            fread(url_buff, sizeof(char), URL_STORE_MAX_URL_LEN, fd);
            fseek(fd, url_len - URL_STORE_MAX_URL_LEN, SEEK_CUR);
            url_len = URL_STORE_MAX_URL_LEN;
        } else {
            fread(url_buff, sizeof(char), url_len, fd);
        }
        
        url_count++;
        string url(url_buff, url_len);

        if (url_count <= 5) {
            fprintf(stderr, "[URL_STORE] url[%zu] len=%u: '%.*s'\n",
                    url_count, url_len, static_cast<int>(url_len > 120 ? 120 : url_len), url_buff);
        }

        UrlShard& shard = get_shard(url);

        auto& url_data = shard.url_data;
        unique_url_count.fetch_add(1, std::memory_order_relaxed);

        fread(&url_data[url].num_encountered, sizeof(uint32_t), 1, fd);
        fread(&url_data[url].seed_distance, sizeof(uint16_t), 1, fd);
        fread(&url_data[url].eot, sizeof(uint16_t), 1, fd);

        size_t title_len;
        fread(&title_len, sizeof(size_t), 1, fd);

        if (title_len > MAX_TITLELEN_MEMORY) {
            fprintf(stderr, "[URL_STORE] CORRUPT at url[%zu]: title_len=%zu, url='%.*s', file pos=%ld\n",
                    url_count, title_len,
                    static_cast<int>(url.size() > 120 ? 120 : url.size()), url.data(),
                    ftell(fd));
            break;
        }
        char* title_buf = new char[title_len];
        fread(title_buf, sizeof(char), title_len, fd);
        url_data[url].title = string(title_buf, title_len);
        delete[] title_buf;
        
        fread(&url_data[url].eod, sizeof(uint16_t), 1, fd);
        fread(&url_data[url].domain_dist, sizeof(uint16_t), 1, fd);
        fread(&url_data[url].crawled, sizeof(bool), 1, fd);

        uint32_t num_anchor_freqs;
        fread(&num_anchor_freqs, sizeof(uint32_t), 1, fd);

        if (url_count <= 5) {
            fprintf(stderr, "[URL_STORE]   url[%zu] title_len=%zu, num_anchor_freqs=%u, file pos=%ld\n",
                    url_count, title_len, num_anchor_freqs, ftell(fd));
        }

        for (uint32_t i = 0; i < num_anchor_freqs; i++) {
            uint32_t anchor_id, freq;
            fread(&anchor_id, sizeof(uint32_t), 1, fd);
            fread(&freq, sizeof(uint32_t), 1, fd);
            url_data[url].anchor_freqs[anchor_id] = freq;
        }
    }

    fclose(fd);
    size_t loaded = unique_url_count.load(std::memory_order_relaxed);
    fprintf(stderr, "[URL_STORE] readFromFile complete: loaded %zu URLs, %zu anchors from %s\n",
            loaded, id_to_anchor.size(), fileName.data());
    fflush(stderr);
}