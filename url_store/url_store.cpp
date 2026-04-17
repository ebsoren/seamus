#include "url_store.h"
#include "../crawler/domain_carousel.h"
#include "../lib/rpc_urlstore.h"
#include "../lib/utils.h"
#include "../lib/algorithm.h"
#include "../lib/Frontier.h"
#include "../lib/logger.h"
#include "../lib/url_filter.h"
#include <optional>
#include <vector>


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
    string lurl = lowercase_url(url);
    UrlShard& us = get_shard(lurl);
    std::lock_guard<std::mutex> lg(us.mtx);
    UrlData* url_data_ptr = us.findUrlData(lurl);
    if (url_data_ptr == nullptr) {
        // If URL priority is low enough that link will never be crawled, don't
        // add to URLStore
        if (priority >= PRIORITY_BUCKETS) {
            logger::debug("updateUrl: dropping url due to low priority (priority=%zu): %s", priority, lurl.data());
            return false;
        }
        if (is_nsfw_url(lurl)) {
            logger::debug("updateUrl: dropping nsfw url: %s", lurl.data());
            return false;
        }
        if (unique_url_count.load(std::memory_order_relaxed) >= MAX_STORE_URLS) {
            logger::warn("UrlStore has reached max capacity, not adding new URL: %s", lurl.data());
            return false;
        }
        return addUrl_unlocked(lurl, anchor_texts, seed_distance, domain_distance, num_encountered);
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
    string lurl = lowercase_url(url);
    UrlShard& us = get_shard(lurl);
    std::lock_guard<std::mutex> lg(us.mtx);
    UrlData* url_data_ptr = us.findUrlData(lurl);
    if (!url_data_ptr) return false;

    url_data_ptr->eot = eot;
    return true;
}

bool UrlStore::updateTitle(const string& url, string& title) {
    string lurl = lowercase_url(url);
    UrlShard& us = get_shard(lurl);
    std::lock_guard<std::mutex> lg(us.mtx);
    UrlData* url_data_ptr = us.findUrlData(lurl);
    if (!url_data_ptr) return false;

    url_data_ptr->title = ::move(title);
    return true;
}


bool UrlStore::updateBodyLen(const string& url, const uint16_t eod) {
    string lurl = lowercase_url(url);
    UrlShard& us = get_shard(lurl);
    std::lock_guard<std::mutex> lg(us.mtx);
    UrlData* url_data_ptr = us.findUrlData(lurl);
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
    logger::error("[URL_STORE] persist(final=%d) starting, unique_url_count=%zu",
            final_persist ? 1 : 0,
            unique_url_count.load(std::memory_order_relaxed));
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

    size_t urls_written = 0, urls_skipped_oversized = 0, urls_skipped_not_crawled = 0, urls_total = 0;
    size_t crawled_count = 0;
    for (size_t i = 0; i < URL_NUM_SHARDS; i++) {
        auto& curr_shard = *(shards + i);
        std::lock_guard<std::mutex> lock(curr_shard.mtx);
        auto& url_data = curr_shard.url_data;
        for (const auto& slot : url_data) {
            urls_total++;
            if (slot.value.crawled) crawled_count++;
            const string& url = slot.key;
            if (url.size() > URL_STORE_MAX_URL_LEN) { urls_skipped_oversized++; continue; }
            if (!slot.value.crawled) urls_skipped_not_crawled++;
            if (final_persist && !slot.value.crawled) continue;
            urls_written++;

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

    logger::error("[URL_STORE] persist(final=%d) done: total=%zu crawled=%zu written=%zu skipped_not_crawled=%zu skipped_oversized=%zu file_size=%ld",
            final_persist ? 1 : 0, urls_total, crawled_count, urls_written,
            urls_skipped_not_crawled, urls_skipped_oversized, ftell(fd));
    fclose(fd);

    string final_file = string::join("", URL_STORE_OUTPUT_DIR_STR, "/urlstore.txt");
    int rc = rename(fileName.data(), final_file.data());
    if (rc != 0) {
        perror("Error renaming urlstore file");
    }
}

// Temporary record used during parallel urlstore load. Points into the
// file buffer — no copies until the actual shard insertion.
struct UrlRecord {
    const char* url;  uint32_t url_len;
    uint32_t num_encountered;
    uint16_t seed_distance, eot, eod, domain_dist;
    const char* title; size_t title_len;
    bool crawled;
    const uint8_t* anchor_freq_data; uint32_t num_anchor_freqs;
};

void UrlStore::readFromFile() {
    string fileName = string::join("", URL_STORE_OUTPUT_DIR_STR, "/urlstore.txt");

    FILE* fd = fopen(fileName.data(), "rb");
    if (fd == nullptr) {
        logger::error("[URL_STORE] FAILED to open %s for reading", fileName.data());
        return;
    }

    // Read entire file into memory in one shot
    fseek(fd, 0, SEEK_END);
    size_t file_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);

    uint8_t* buf = new uint8_t[file_size];
    size_t rd = fread(buf, 1, file_size, fd);
    fclose(fd);
    if (rd != file_size) {
        logger::error("[URL_STORE] short read: got %zu / %zu bytes", rd, file_size);
        delete[] buf;
        return;
    }
    logger::error("[URL_STORE] read %zu bytes into memory from %s", file_size, fileName.data());

    const uint8_t* p = buf;
    const uint8_t* end = buf + file_size;

    // --- Parse anchors (sequential, fast) ---
    if (p + sizeof(uint32_t) > end) { delete[] buf; return; }
    uint32_t num_anchor_texts;
    memcpy(&num_anchor_texts, p, sizeof(uint32_t)); p += sizeof(uint32_t);
    logger::error("[URL_STORE] reading %u anchor texts", num_anchor_texts);

    size_t oversized_anchors = 0;
    for (uint32_t i = 0; i < num_anchor_texts; i++) {
        if (p + sizeof(uint32_t) > end) break;
        uint32_t anchor_len;
        memcpy(&anchor_len, p, sizeof(uint32_t)); p += sizeof(uint32_t);

        uint32_t actual_len = anchor_len;
        if (anchor_len > URL_STORE_MAX_ANCHOR_TEXT_LEN) {
            actual_len = URL_STORE_MAX_ANCHOR_TEXT_LEN;
            oversized_anchors++;
        }
        if (p + anchor_len > end) break;
        id_to_anchor.push_back(string((const char*)p, actual_len));
        anchor_to_id[string((const char*)p, actual_len)] = id_to_anchor.size() - 1;
        p += anchor_len;
    }
    logger::error("[URL_STORE] anchors loaded: %zu total, %zu oversized", id_to_anchor.size(), oversized_anchors);

    // --- Parse URL records into per-shard buckets (sequential scan) ---
    std::vector<std::vector<UrlRecord>> shard_buckets(URL_NUM_SHARDS);
    size_t url_count = 0;

    while (p + sizeof(uint32_t) <= end) {
        uint32_t url_len;
        memcpy(&url_len, p, sizeof(uint32_t)); p += sizeof(uint32_t);
        uint32_t actual_url_len = url_len > URL_STORE_MAX_URL_LEN ? URL_STORE_MAX_URL_LEN : url_len;
        if (p + url_len > end) break;

        // Lowercase in-place in the buffer (safe, we own it)
        char* url_ptr = (char*)p;
        for (uint32_t ci = 0; ci < actual_url_len; ci++) {
            if (url_ptr[ci] >= 'A' && url_ptr[ci] <= 'Z') url_ptr[ci] += 32;
        }
        p += url_len;

        UrlRecord rec;
        rec.url = url_ptr;
        rec.url_len = actual_url_len;

        if (p + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(size_t) > end) break;
        memcpy(&rec.num_encountered, p, sizeof(uint32_t)); p += sizeof(uint32_t);
        memcpy(&rec.seed_distance, p, sizeof(uint16_t));   p += sizeof(uint16_t);
        memcpy(&rec.eot, p, sizeof(uint16_t));              p += sizeof(uint16_t);

        size_t title_len;
        memcpy(&title_len, p, sizeof(size_t)); p += sizeof(size_t);

        if (p + title_len > end) break;
        if (title_len > MAX_TITLELEN_MEMORY) {
            rec.title = nullptr;
            rec.title_len = 0;
        } else {
            rec.title = (const char*)p;
            rec.title_len = title_len;
        }
        p += title_len;

        if (p + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(bool) + sizeof(uint32_t) > end) break;
        memcpy(&rec.eod, p, sizeof(uint16_t));          p += sizeof(uint16_t);
        memcpy(&rec.domain_dist, p, sizeof(uint16_t));   p += sizeof(uint16_t);
        memcpy(&rec.crawled, p, sizeof(bool));            p += sizeof(bool);

        memcpy(&rec.num_anchor_freqs, p, sizeof(uint32_t)); p += sizeof(uint32_t);

        size_t anchor_bytes = rec.num_anchor_freqs * 2 * sizeof(uint32_t);
        if (p + anchor_bytes > end) break;
        rec.anchor_freq_data = p;
        p += anchor_bytes;

        // Hash to shard
        size_t shard_id = hasher(string(rec.url, rec.url_len)) % URL_NUM_SHARDS;
        shard_buckets[shard_id].push_back(rec);
        url_count++;
        if (url_count % 1000000 == 0) {
            logger::error("[URL_STORE] parsed %zu URLs so far...", url_count);
        }
    }
    logger::error("[URL_STORE] parsed %zu URL records, inserting into shards with %u threads",
            url_count, URL_STORE_NUM_THREADS);

    // --- Parallel shard insertion: each thread owns exclusive shards ---
    size_t num_threads = URL_STORE_NUM_THREADS;
    if (num_threads > URL_NUM_SHARDS) num_threads = URL_NUM_SHARDS;
    size_t shards_per_thread = URL_NUM_SHARDS / num_threads;

    std::vector<std::thread> threads;
    for (size_t t = 0; t < num_threads; t++) {
        size_t shard_start = t * shards_per_thread;
        size_t shard_end = (t == num_threads - 1) ? URL_NUM_SHARDS : shard_start + shards_per_thread;

        threads.emplace_back([this, &shard_buckets, shard_start, shard_end, t]() {
            size_t thread_total = 0;
            for (size_t s = shard_start; s < shard_end; s++) {
                auto& bucket = shard_buckets[s];
                auto& url_data = shards[s].url_data;
                thread_total += bucket.size();
                for (const UrlRecord& rec : bucket) {
                    string url(rec.url, rec.url_len);
                    UrlData& data = url_data[url];
                    data.num_encountered = rec.num_encountered;
                    data.seed_distance = rec.seed_distance;
                    data.eot = rec.eot;
                    data.eod = rec.eod;
                    data.domain_dist = rec.domain_dist;
                    data.crawled = rec.crawled;
                    if (rec.title != nullptr) {
                        data.title = string(rec.title, rec.title_len);
                    }
                    const uint8_t* ap = rec.anchor_freq_data;
                    for (uint32_t i = 0; i < rec.num_anchor_freqs; i++) {
                        uint32_t anchor_id, freq;
                        memcpy(&anchor_id, ap, sizeof(uint32_t)); ap += sizeof(uint32_t);
                        memcpy(&freq, ap, sizeof(uint32_t));      ap += sizeof(uint32_t);
                        data.anchor_freqs[anchor_id] = freq;
                    }
                }
            }
            logger::error("[URL_STORE] thread %zu done: shards %zu-%zu, %zu URLs inserted",
                    t, shard_start, shard_end - 1, thread_total);
        });
    }
    for (auto& t : threads) t.join();

    unique_url_count.store(url_count, std::memory_order_relaxed);
    logger::error("[URL_STORE] readFromFile complete: loaded %zu URLs, %zu anchors from %s",
            url_count, id_to_anchor.size(), fileName.data());

    delete[] buf;
}