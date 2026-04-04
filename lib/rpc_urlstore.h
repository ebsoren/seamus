#pragma once

#include "string.h"
#include "vector.h"
#include "rpc_common.h"
#include "utils.h"
#include "unordered_map.h"

#include <cstdint>
#include <memory>
#include <optional>

/*
Currently, there functionality is divided into a couple categories:
    - Sending URLStore update/write requests from crawlers
    - URLStore read/data getters and batch getters
*/

struct AnchorData {
    uint32_t anchor_id;
    uint32_t freq;
};

struct UrlData {
    unordered_map<uint32_t, uint32_t> anchor_freqs{8, 0.65};             // anchor text id to frequency since its last update (potentially size >1)
    uint32_t num_encountered;                            // Number of times this URL has been encountered
    uint16_t seed_distance;                              // Distance from seed list
    uint16_t domain_dist;                                // Domain distance from seed list TODO(charlie): implement this feature
    uint16_t eot;                                        // End of title
    // TODO(charlie): add title content, not eot
    string title = string("", 0);
    uint16_t eod;                                        // End of description
    bool crawled = false;                                // Whether this URL has actually been fetched
};


// Request sent to end server hosting the (sharded) dynamic URL data when a new URL is encountered
struct URLStoreUpdateRequest {
    string url = string("", 0);                         // URL: primary identifier

    vector<string> anchor_text;         // Vector of anchor text strings used to refer to this URL since its last update (potentially size >1)
    uint32_t num_encountered;           // Number of additional times this URL has been encountered since its last update
    uint16_t seed_list_url_hops;        // Found distance in url hops from seed list (updated if lower than current)
    uint16_t seed_list_domain_hops;     // Found distance in domain hops from the seed list (updated if lower than current)
};

struct BatchURLStoreUpdateRequest {
    vector<URLStoreUpdateRequest> reqs;
};

struct BatchURLStoreUpdateResponse {
    // For future use if we want to send back any data (e.g. canonical URL, updated metadata, etc.)
};

// RPC getter/response for URL data, same send structure as above
// Responses are batched as a parallel vector to the urls in the request
struct BatchURLStoreDataRequest {
    vector<string> urls;
};

struct BatchURLStoreDataResponse {
    vector<UrlData> resps;
};

inline size_t anchor_text_total_size(const vector<string>& anchor_texts) {
    size_t total = 0;
    for (const string& anchor : anchor_texts) {
        total += anchor.size();
    }
    return static_cast<size_t>(total);
}

// Helper to serialize + send a BatchURLStoreUpdateRequest over network
inline bool send_batch_urlstore_update(const string& host, uint16_t port, const BatchURLStoreUpdateRequest& batch) {
    // Compute total buffer size:
    // 4 bytes for request count
    // Per request: 4 + url.size() + 4 + 4 + anchor_text_total_size(req.anchor_text) + 4 + 4 + 4
    size_t total = sizeof(uint32_t);
    for (size_t i = 0; i < batch.reqs.size(); i++) {
        const auto& req = batch.reqs[i];
        total += sizeof(uint32_t) + req.url.size()
               + sizeof(uint32_t)
               + req.anchor_text.size() * sizeof(uint32_t) + anchor_text_total_size(req.anchor_text)
               + sizeof(uint32_t)
               + sizeof(uint16_t) * 2;
    }

    auto buf = std::make_unique<char[]>(total);
    size_t off = 0;

    // Write request count
    uint32_t count = htonl(static_cast<uint32_t>(batch.reqs.size()));
    std::memcpy(buf.get() + off, &count, sizeof(uint32_t));
    off += sizeof(uint32_t);

    for (size_t i = 0; i < batch.reqs.size(); i++) {
        const auto& req = batch.reqs[i];

        // url (length-prefixed)
        uint32_t url_len = htonl(static_cast<uint32_t>(req.url.size()));
        std::memcpy(buf.get() + off, &url_len, sizeof(uint32_t));
        off += sizeof(uint32_t);
        std::memcpy(buf.get() + off, req.url.data(), req.url.size());
        off += req.url.size();

        // anchor_text (length-prefixed)
        // send length of vector first then have recv read in this size and loop for that
        uint32_t anchor_count = htonl(static_cast<uint32_t>(req.anchor_text.size()));
        std::memcpy(buf.get() + off, &anchor_count, sizeof(uint32_t));
        off += sizeof(uint32_t);
        for (const string& anchor : req.anchor_text) {
            uint32_t anchor_len = htonl(static_cast<uint32_t>(anchor.size()));
            std::memcpy(buf.get() + off, &anchor_len, sizeof(uint32_t));
            off += sizeof(uint32_t);
            std::memcpy(buf.get() + off, anchor.data(), anchor.size());
            off += anchor.size();
        }

        // num_encountered
        uint32_t enc = htonl(req.num_encountered);
        std::memcpy(buf.get() + off, &enc, sizeof(uint32_t));
        off += sizeof(uint32_t);

        // seed_list_url_hops
        uint16_t url_hops = htons(req.seed_list_url_hops);
        std::memcpy(buf.get() + off, &url_hops, sizeof(uint16_t));
        off += sizeof(uint16_t);

        // seed_list_domain_hops
        uint16_t domain_hops = htons(req.seed_list_domain_hops);
        std::memcpy(buf.get() + off, &domain_hops, sizeof(uint16_t));
        off += sizeof(uint16_t);
    }

    return send_buffer(host, port, buf.get(), total);
}


// Helper to serialize + send a BatchURLStoreDataRequest over network
inline bool send_batch_urlstore_data_request(const string& host, uint16_t port, const BatchURLStoreDataRequest& batch) {
    size_t total = sizeof(uint32_t);
    for (size_t i = 0; i < batch.urls.size(); i++) {
        total += sizeof(uint32_t) + batch.urls[i].size();
    }

    auto buf = std::make_unique<char[]>(total);
    size_t off = 0;

    uint32_t count = htonl(static_cast<uint32_t>(batch.urls.size()));
    std::memcpy(buf.get() + off, &count, sizeof(uint32_t));
    off += sizeof(uint32_t);

    for (size_t i = 0; i < batch.urls.size(); i++) {
        uint32_t url_len = htonl(static_cast<uint32_t>(batch.urls[i].size()));
        std::memcpy(buf.get() + off, &url_len, sizeof(uint32_t));
        off += sizeof(uint32_t);
        std::memcpy(buf.get() + off, batch.urls[i].data(), batch.urls[i].size());
        off += batch.urls[i].size();
    }

    return send_buffer(host, port, buf.get(), total);
}


// Read an AnchorData from fd
inline std::optional<AnchorData> recv_anchor_data(int fd) {
    AnchorData a{};
    if (!recv_u32(fd, a.anchor_id)) return std::nullopt;
    if (!recv_u32(fd, a.freq)) return std::nullopt;
    return a;
}


// Read a UrlData from fd
inline std::optional<UrlData> recv_url_data(int fd) {
    uint32_t anchor_count;
    if (!recv_u32(fd, anchor_count)) return std::nullopt;

    UrlData d{};
    for (uint32_t i = 0; i < anchor_count; i++) {
        auto a = recv_anchor_data(fd);
        if (!a) return std::nullopt;
        d.anchor_freqs[a->anchor_id] = a->freq;
    }

    if (!recv_u32(fd, d.num_encountered)) return std::nullopt;
    if (!recv_u16(fd, d.seed_distance)) return std::nullopt;
    if (!recv_u16(fd, d.domain_dist)) return std::nullopt;
    if (!recv_u16(fd, d.eot)) return std::nullopt;
    if (!recv_u16(fd, d.eod)) return std::nullopt;

    return std::optional<UrlData>(std::move(d));
}


// Helper to serialize + send a BatchURLStoreDataResponse over network
inline bool send_batch_urlstore_data_response(const string& host, uint16_t port, BatchURLStoreDataResponse& batch) {
    // Compute total buffer size:
    // 4 bytes for response count
    // Per UrlData: 4 (anchor count) + 8 per anchor + 4 (num_encountered) + 2*4 (seed_distance, domain_dist, eot, eod)
    size_t total = sizeof(uint32_t);
    for (size_t i = 0; i < batch.resps.size(); i++) {
        total += sizeof(uint32_t)
               + batch.resps[i].anchor_freqs.size() * (sizeof(uint32_t) * 2)
               + sizeof(uint32_t)
               + sizeof(uint16_t) * 4;
    }

    auto buf = std::make_unique<char[]>(total);
    size_t off = 0;

    uint32_t count = htonl(static_cast<uint32_t>(batch.resps.size()));
    std::memcpy(buf.get() + off, &count, sizeof(uint32_t));
    off += sizeof(uint32_t);

    for (size_t i = 0; i < batch.resps.size(); i++) {
        auto& d = batch.resps[i];

        // anchor_freqs (count-prefixed)
        uint32_t anchor_count = htonl(static_cast<uint32_t>(d.anchor_freqs.size()));
        std::memcpy(buf.get() + off, &anchor_count, sizeof(uint32_t));
        off += sizeof(uint32_t);

        for (auto it = d.anchor_freqs.begin(); it != d.anchor_freqs.end(); ++it) {
            const auto& tuple = *it;
            uint32_t aid = htonl(tuple.key);
            std::memcpy(buf.get() + off, &aid, sizeof(uint32_t));
            off += sizeof(uint32_t);

            uint32_t freq = htonl(tuple.value);
            std::memcpy(buf.get() + off, &freq, sizeof(uint32_t));
            off += sizeof(uint32_t);
        }

        uint32_t enc = htonl(d.num_encountered);
        std::memcpy(buf.get() + off, &enc, sizeof(uint32_t));
        off += sizeof(uint32_t);

        uint16_t sd = htons(d.seed_distance);
        std::memcpy(buf.get() + off, &sd, sizeof(uint16_t));
        off += sizeof(uint16_t);

        uint16_t dd = htons(d.domain_dist);
        std::memcpy(buf.get() + off, &dd, sizeof(uint16_t));
        off += sizeof(uint16_t);

        uint16_t eot = htons(d.eot);
        std::memcpy(buf.get() + off, &eot, sizeof(uint16_t));
        off += sizeof(uint16_t);

        uint16_t eod = htons(d.eod);
        std::memcpy(buf.get() + off, &eod, sizeof(uint16_t));
        off += sizeof(uint16_t);
    }

    return send_buffer(host, port, buf.get(), total);
}


// Helper to deserialize a BatchURLStoreDataResponse from a socket fd
inline std::optional<BatchURLStoreDataResponse> recv_batch_urlstore_data_response(int fd) {
    uint32_t count;
    if (!recv_u32(fd, count)) return std::nullopt;

    BatchURLStoreDataResponse batch;
    for (uint32_t i = 0; i < count; i++) {
        auto d = recv_url_data(fd);
        if (!d) return std::nullopt;
        batch.resps.push_back(std::move(*d));
    }

    return std::optional<BatchURLStoreDataResponse>(std::move(batch));
}


// Helper to deserialize a BatchURLStoreDataRequest from a socket fd
inline std::optional<BatchURLStoreDataRequest> recv_batch_urlstore_data_request(int fd) {
    uint32_t count;
    if (!recv_u32(fd, count)) return std::nullopt;

    BatchURLStoreDataRequest batch;
    for (uint32_t i = 0; i < count; i++) {
        auto url = recv_string(fd);
        if (!url) return std::nullopt;
        batch.urls.push_back(std::move(*url));
    }

    return std::optional<BatchURLStoreDataRequest>(std::move(batch));
}


// Helper to deserialize a BatchURLStoreUpdateRequest from a socket fd
inline std::optional<BatchURLStoreUpdateRequest> recv_batch_urlstore_update(int fd) {
    uint32_t count;
    if (!recv_u32(fd, count)) return std::nullopt;

    BatchURLStoreUpdateRequest batch;
    for (uint32_t i = 0; i < count; i++) {
        auto url = recv_string(fd);
        if (!url) return std::nullopt;

        vector<string> anchor_texts;
        uint32_t anchor_count;
        if (!recv_u32(fd, anchor_count)) return std::nullopt;
        for (size_t j = 0; j < anchor_count; j++) {
            auto anchor = recv_string(fd);
            if (!anchor) return std::nullopt;
            anchor_texts.push_back(std::move(*anchor));
        }

        URLStoreUpdateRequest req{std::move(*url), move(anchor_texts), 0, 0, 0};

        if (!recv_u32(fd, req.num_encountered)) return std::nullopt;
        if (!recv_u16(fd, req.seed_list_url_hops)) return std::nullopt;
        if (!recv_u16(fd, req.seed_list_domain_hops)) return std::nullopt;

        batch.reqs.push_back(std::move(req));
    }

    return std::optional<BatchURLStoreUpdateRequest>(std::move(batch));
}
