#pragma once

#include "string.h"
#include "vector.h"
#include "rpc_common.h"
#include <cstdint>
#include <optional>


struct AnchorData {
    uint32_t anchor_id;
    uint32_t freq;
};


// TODO: decide if vector is better than hashmap for anchor_freqs (cache locality vs. O(1) average lookup)
struct UrlData {
    vector<AnchorData> anchor_freqs;                     // anchor text id to frequency since its last update (potentially size >1)
    uint32_t num_encountered;                            // Number of additional times this URL has been
    uint16_t seed_distance;                              // Distance from seed list
    uint16_t domain_dist;                                // Domain distance from seed list TODO(charlie): implement this feature
    uint16_t eot;                                        // End of title
    uint16_t eod;                                        // End of description
};


// Request sent to end server hosting the (sharded) dynamic URL data when a new URL is encountered
struct URLStoreUpdateRequest {
    string url;                         // URL: primary identifier
    string anchor_text;                 // Vector of anchor text strings used to refer to this URL since its last update (potentially size >1)
    uint32_t num_encountered;           // Number of additional times this URL has been encountered since its last update
    uint32_t seed_list_url_hops;        // Found distance in url hops from seed list (updated if lower than current)
    uint32_t seed_list_domain_hops;     // Found distance in domain hops from the seed list (updated if lower than current)
};

struct BatchURLStoreUpdateRequest {
    vector<URLStoreUpdateRequest> reqs;
};


// RPC getter/response for URL data, same send structure as above
// Responses are batched as a parallel vector to the urls in the request
struct BatchURLStoreDataRequest {
    vector<string> urls;
};

struct BatchURLStoreDataResponse {
    vector<UrlData> resps;
};


// Helper to serialize + send a BatchURLStoreUpdateRequest over network
inline bool send_batch_urlstore_update(const string& host, uint16_t port, const BatchURLStoreUpdateRequest& batch) {
    // Compute total buffer size:
    // 4 bytes for request count
    // Per request: 4 + url.size() + 4 + anchor_text.size() + 4 + 4 + 4
    size_t total = sizeof(uint32_t);
    for (size_t i = 0; i < batch.reqs.size(); i++) {
        const auto& req = batch.reqs[i];
        total += sizeof(uint32_t) + req.url.size()
               + sizeof(uint32_t) + req.anchor_text.size()
               + sizeof(uint32_t) * 3;
    }

    char* buf = new char[total];
    size_t off = 0;

    // Write request count
    uint32_t count = htonl(static_cast<uint32_t>(batch.reqs.size()));
    std::memcpy(buf + off, &count, sizeof(uint32_t));
    off += sizeof(uint32_t);

    for (size_t i = 0; i < batch.reqs.size(); i++) {
        const auto& req = batch.reqs[i];

        // url (length-prefixed)
        uint32_t url_len = htonl(static_cast<uint32_t>(req.url.size()));
        std::memcpy(buf + off, &url_len, sizeof(uint32_t));
        off += sizeof(uint32_t);
        std::memcpy(buf + off, req.url.data(), req.url.size());
        off += req.url.size();

        // anchor_text (length-prefixed)
        uint32_t anchor_len = htonl(static_cast<uint32_t>(req.anchor_text.size()));
        std::memcpy(buf + off, &anchor_len, sizeof(uint32_t));
        off += sizeof(uint32_t);
        std::memcpy(buf + off, req.anchor_text.data(), req.anchor_text.size());
        off += req.anchor_text.size();

        // num_encountered
        uint32_t enc = htonl(req.num_encountered);
        std::memcpy(buf + off, &enc, sizeof(uint32_t));
        off += sizeof(uint32_t);

        // seed_list_url_hops
        uint32_t url_hops = htonl(req.seed_list_url_hops);
        std::memcpy(buf + off, &url_hops, sizeof(uint32_t));
        off += sizeof(uint32_t);

        // seed_list_domain_hops
        uint32_t domain_hops = htonl(req.seed_list_domain_hops);
        std::memcpy(buf + off, &domain_hops, sizeof(uint32_t));
        off += sizeof(uint32_t);
    }

    bool ok = send_buffer(host, port, buf, total);
    delete[] buf;
    return ok;
}


// Helper to serialize + send a BatchURLStoreDataRequest over network
inline bool send_batch_urlstore_data_request(const string& host, uint16_t port, const BatchURLStoreDataRequest& batch) {
    size_t total = sizeof(uint32_t);
    for (size_t i = 0; i < batch.urls.size(); i++) {
        total += sizeof(uint32_t) + batch.urls[i].size();
    }

    char* buf = new char[total];
    size_t off = 0;

    uint32_t count = htonl(static_cast<uint32_t>(batch.urls.size()));
    std::memcpy(buf + off, &count, sizeof(uint32_t));
    off += sizeof(uint32_t);

    for (size_t i = 0; i < batch.urls.size(); i++) {
        uint32_t url_len = htonl(static_cast<uint32_t>(batch.urls[i].size()));
        std::memcpy(buf + off, &url_len, sizeof(uint32_t));
        off += sizeof(uint32_t);
        std::memcpy(buf + off, batch.urls[i].data(), batch.urls[i].size());
        off += batch.urls[i].size();
    }

    bool ok = send_buffer(host, port, buf, total);
    delete[] buf;
    return ok;
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
        d.anchor_freqs.push_back(*a);
    }

    if (!recv_u32(fd, d.num_encountered)) return std::nullopt;
    if (!recv_u16(fd, d.seed_distance)) return std::nullopt;
    if (!recv_u16(fd, d.domain_dist)) return std::nullopt;
    if (!recv_u16(fd, d.eot)) return std::nullopt;
    if (!recv_u16(fd, d.eod)) return std::nullopt;

    return std::optional<UrlData>(std::move(d));
}


// Helper to serialize + send a BatchURLStoreDataResponse over network
inline bool send_batch_urlstore_data_response(const string& host, uint16_t port, const BatchURLStoreDataResponse& batch) {
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

    char* buf = new char[total];
    size_t off = 0;

    uint32_t count = htonl(static_cast<uint32_t>(batch.resps.size()));
    std::memcpy(buf + off, &count, sizeof(uint32_t));
    off += sizeof(uint32_t);

    for (size_t i = 0; i < batch.resps.size(); i++) {
        const auto& d = batch.resps[i];

        // anchor_freqs (count-prefixed)
        uint32_t anchor_count = htonl(static_cast<uint32_t>(d.anchor_freqs.size()));
        std::memcpy(buf + off, &anchor_count, sizeof(uint32_t));
        off += sizeof(uint32_t);

        for (size_t j = 0; j < d.anchor_freqs.size(); j++) {
            uint32_t aid = htonl(d.anchor_freqs[j].anchor_id);
            std::memcpy(buf + off, &aid, sizeof(uint32_t));
            off += sizeof(uint32_t);

            uint32_t freq = htonl(d.anchor_freqs[j].freq);
            std::memcpy(buf + off, &freq, sizeof(uint32_t));
            off += sizeof(uint32_t);
        }

        uint32_t enc = htonl(d.num_encountered);
        std::memcpy(buf + off, &enc, sizeof(uint32_t));
        off += sizeof(uint32_t);

        uint16_t sd = htons(d.seed_distance);
        std::memcpy(buf + off, &sd, sizeof(uint16_t));
        off += sizeof(uint16_t);

        uint16_t dd = htons(d.domain_dist);
        std::memcpy(buf + off, &dd, sizeof(uint16_t));
        off += sizeof(uint16_t);

        uint16_t eot = htons(d.eot);
        std::memcpy(buf + off, &eot, sizeof(uint16_t));
        off += sizeof(uint16_t);

        uint16_t eod = htons(d.eod);
        std::memcpy(buf + off, &eod, sizeof(uint16_t));
        off += sizeof(uint16_t);
    }

    bool ok = send_buffer(host, port, buf, total);
    delete[] buf;
    return ok;
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

        auto anchor = recv_string(fd);
        if (!anchor) return std::nullopt;

        URLStoreUpdateRequest req{std::move(*url), std::move(*anchor), 0, 0, 0};

        if (!recv_u32(fd, req.num_encountered)) return std::nullopt;
        if (!recv_u32(fd, req.seed_list_url_hops)) return std::nullopt;
        if (!recv_u32(fd, req.seed_list_domain_hops)) return std::nullopt;

        batch.reqs.push_back(std::move(req));
    }

    return std::optional<BatchURLStoreUpdateRequest>(std::move(batch));
}
