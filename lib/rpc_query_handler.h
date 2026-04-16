#pragma once

#include "../lib/string.h"
#include "../lib/vector.h"
#include "../lib/rpc_common.h"
#include "../ranker/Ranker.h"

#include <cstdint>
#include <memory>
#include <optional>

struct LeanPage {
    string url = string("");
    string title = string("");
    double score; 
};


struct LeanPageResponse {
    // clarifier: multiple RankedPage structs can be returned a a result to the client query handler
    // this represents all the pages a WHOLE query is valid on
    vector<LeanPage> pages;
};

// returns a RankedPageResponse
inline LeanPageResponse send_recv_query_data(const string& host, const uint16_t port, const string& query) {
    int sock_fd = connect_to_host(host, port);
    if (sock_fd < 0) return LeanPageResponse(); // Connection failed

    // 2. Send the word request (using the new mirror helper)
    if (!send_string(sock_fd, query)) {
        close(sock_fd);
        return LeanPageResponse(); // Send failed
    }

    uint32_t num_pages;
    if (!recv_u32(sock_fd, num_pages)) {
        close(sock_fd);
        return LeanPageResponse(); // Server dropped connection
    }

    LeanPageResponse remote_hits;
    uint32_t page_count;
    if (!recv_u32(sock_fd, page_count)) {
        close(sock_fd);
        return LeanPageResponse();
    }

    for (uint32_t i = 0; i < page_count; ++i) {
        // recv title, url, then score in that order
        LeanPage curr;
        std::optional<string> title = recv_string(sock_fd);
        std::optional<string> url = recv_string(sock_fd);
        std::optional<double> score = recv_double(sock_fd);
        if (title == std::nullopt || url == std::nullopt || score == std::nullopt) continue;
        curr.title = std::move(url.value());
        curr.url = std::move(url.value());
        curr.score = score.value();
        remote_hits.pages.push_back(std::move(curr));
    }

    close(sock_fd);
    return remote_hits;
}

// vector<RankedPage> pages
inline bool send_query_response(const uint16_t fd, const LeanPageResponse& results) {
    size_t total = sizeof(uint32_t);
    for (const LeanPage& page : results.pages) { 
        total += sizeof(uint32_t) + page.title.size();
        total += sizeof(uint32_t) + page.url.size();
        total += sizeof(double);
    }

    auto buf = std::make_unique<char[]>(total);
    size_t off = 0;

    uint32_t page_count = htonl(static_cast<uint32_t>(results.pages.size()));
    std::memcpy(buf.get() + off, &page_count, sizeof(uint32_t));
    off += sizeof(uint32_t);

    for (const LeanPage& page : results.pages) {
        // Serialize each field of the LeanPage object
        uint32_t title_len = htonl(static_cast<uint32_t>(page.title.size()));
        std::memcpy(buf.get() + off, &title_len, sizeof(uint32_t));
        off += sizeof(uint32_t);
        std::memcpy(buf.get() + off, page.title.data(), page.title.size());
        off += page.title.size();

        uint32_t url_len = htonl(static_cast<uint32_t>(page.url.size()));
        std::memcpy(buf.get() + off, &url_len, sizeof(uint32_t));
        off += sizeof(uint32_t);
        std::memcpy(buf.get() + off, page.url.data(), page.url.size());
        off += page.url.size();

        uint64_t int_rep = std::bit_cast<uint64_t>(page.score);
        uint64_t network_val = htobe64(int_rep);
        std::memcpy(buf.get() + off, &network_val, sizeof(uint64_t));
        off += sizeof(double);
    }

    bool result = send_exact(fd, buf.get(), total); 
    return result;
}