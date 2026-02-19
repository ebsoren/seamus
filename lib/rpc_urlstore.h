#pragma once

#include "string.h"
#include "vector.h"
#include "rpc_common.h"
#include <cstdint>
#include <optional>


// Request sent to end server hosting the (sharded) dynamic URL data when a new URL is encountered
// This request is capable of being batched (per URL)
// Distance from seed list is never something that needs to be updated, so it is not included in this RPC
struct URLStoreUpdateRequest {
    string url;                         // URL: primary identifier
    vector<string> anchor_text;         // Vector of anchor text strings used to refer to this URL since its last update (potentially size >1)
    uint32_t num_encountered;           // Number of additional times this URL has been encountered since its last update
};


// Serialize a URLStoreUpdateRequest into a buffer and send it to an end host via an ephemeral socket
// Wire format: [4B url_len][url][4B anchor_count]([4B anchor_len][anchor]...)[4B num_encountered]
// All multi-byte integers are in network byte order.
inline bool send_urlstore_update(const string& host, uint16_t port, const URLStoreUpdateRequest& req) {
    // Compute total buffer size
    size_t total = sizeof(uint32_t) + req.url.size()   // url
                 + sizeof(uint32_t);                    // anchor_count
    for (const string& anchor : req.anchor_text) {
        total += sizeof(uint32_t) + anchor.size();
    }
    total += sizeof(uint32_t);                          // num_encountered

    char* buf = new char[total];
    char* ptr = buf;

    auto write_u32 = [&ptr](uint32_t val) {
        uint32_t net = htonl(val);
        memcpy(ptr, &net, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    };

    auto write_string = [&ptr, &write_u32](const string& s) {
        write_u32(static_cast<uint32_t>(s.size()));
        memcpy(ptr, s.data(), s.size());
        ptr += s.size();
    };

    write_string(req.url);
    write_u32(static_cast<uint32_t>(req.anchor_text.size()));
    for (const string& anchor : req.anchor_text) {
        write_string(anchor);
    }
    write_u32(req.num_encountered);

    bool ok = send_buffer(host, port, buf, total);
    delete[] buf;
    return ok;
}


// Deserialize a URLStoreUpdateRequest from an ephemeral socket fd
// Wire format: [4B url_len][url][4B anchor_count]([4B anchor_len][anchor]...)[4B num_encountered]
// Returns nullopt if the socket closes or errors mid-read
inline std::optional<URLStoreUpdateRequest> recv_urlstore_update(int fd) {
    auto url = recv_string(fd);
    if (!url) return std::nullopt;

    uint32_t anchor_count;
    if (!recv_u32(fd, anchor_count)) return std::nullopt;

    vector<string> anchor_text;
    for (uint32_t i = 0; i < anchor_count; ++i) {
        auto anchor = recv_string(fd);
        if (!anchor) return std::nullopt;
        anchor_text.push_back(std::move(*anchor));
    }

    uint32_t num_encountered;
    if (!recv_u32(fd, num_encountered)) return std::nullopt;

    return URLStoreUpdateRequest{std::move(*url), std::move(anchor_text), num_encountered};
}
