#pragma once

#include "../lib/string.h"
#include "../lib/vector.h"
#include "../lib/rpc_common.h"
#include "../ranker/Ranker.h"

#include <cstdint>
#include <memory>
#include <optional>

struct RankedPageResponse {
    vector<RankedPage> pages;
};

// returns the fd created to pass to recv word response
vector<RankedPage> send_recv_word_data(const string& host, const uint16_t port, const string_view& word, bool is_phrase) {
    int sock_fd = connect_to_host(host, port);
    if (sock_fd < 0) return vector<RankedPage>{}; // Connection failed

    // 2. Send the word request (using the new mirror helper)
    if (!send_stringview(sock_fd, word)) {
        close(sock_fd);
        return vector<RankedPage>{}; // Send failed
    }

    uint32_t num_pages;
    if (!recv_u32(sock_fd, num_pages)) {
        close(sock_fd);
        return vector<RankedPage>{}; // Server dropped connection
    }

    vector<RankedPage> remote_hits;
    // read full response payload and deserialize into vector<RankedPage>

    close(sock_fd);
    return remote_hits;
}

inline bool send_word_response(const uint16_t fd, const RankedPageResponse& results) {
    size_t total = sizeof(uint32_t);
    for (const RankedPage& page : results.pages) {
        total += sizeof(uint32_t) + page.url.size();
        total += sizeof(uint32_t) + page.title.size();
        total += sizeof(int) * 6; // for the 6 int fields in RankedPage
        for (const vector<size_t>& positions : page.word_positions) {
            total += sizeof(uint32_t); // for the number of positions for this word
            total += sizeof(size_t) * positions.size(); // for the positions themselves
        }
        total += sizeof(size_t) * 2; // for doc_len and description_len
        total += sizeof(uint32_t); // for the number of word position vectors
    }

    auto buf = std::make_unique<char[]>(total);
    size_t off = 0;

    uint32_t page_count = htonl(static_cast<uint32_t>(results.pages.size()));
    std::memcpy(buf.get() + off, &page_count, sizeof(uint32_t));
    off += sizeof(uint32_t);

    for (size_t i = 0; i < results.pages.size(); ++i) {
        const RankedPage& page = results.pages[i];
        // Serialize each field of the RankedPage object
        uint32_t url_len = htonl(static_cast<uint32_t>(page.url.size()));
        std::memcpy(buf.get() + off, &url_len, sizeof(uint32_t));
        off += sizeof(uint32_t);
        std::memcpy(buf.get() + off, page.url.data(), page.url.size());
        off += page.url.size();

        uint32_t title_len = htonl(static_cast<uint32_t>(page.title.size()));
        std::memcpy(buf.get() + off, &title_len, sizeof(uint32_t));
        off += sizeof(uint32_t);
        std::memcpy(buf.get() + off, page.title.data(), page.title.size());
        off += page.title.size();

        std::memcpy(buf.get() + off, &page.seed_list_dist, sizeof(int));
        off += sizeof(int);

        std::memcpy(buf.get() + off, &page.domains_from_seed, sizeof(int));
        off += sizeof(int);

        std::memcpy(buf.get() + off, &page.num_unique_words_found_anchor, sizeof(int));
        off += sizeof(int);

        std::memcpy(buf.get() + off, &page.num_unique_words_found_title, sizeof(int));
        off += sizeof(int);

        std::memcpy(buf.get() + off, &page.num_unique_words_found_descr, sizeof(int));
        off += sizeof(int);

        std::memcpy(buf.get() + off, &page.num_unique_words_found_url, sizeof(int));
        off += sizeof(int);

        std::memcpy(buf.get() + off, &page.times_seen, sizeof(int));
        off += sizeof(int);

        uint32_t num_word_pos_vectors = htonl(static_cast<uint32_t>(page.word_positions.size()));
        std::memcpy(buf.get() + off, &num_word_pos_vectors, sizeof(uint32_t));
        off += sizeof(uint32_t);

        for (const vector<size_t>& positions : page.word_positions) {
            uint32_t pos_count = htonl(static_cast<uint32_t>(positions.size()));
            std::memcpy(buf.get() + off, &pos_count, sizeof(uint32_t));
            off += sizeof(uint32_t);
            for (size_t pos : positions) {
                std::memcpy(buf.get() + off, &pos, sizeof(size_t));
                off += sizeof(size_t);
            }
        }

        std::memcpy(buf.get() + off, &page.doc_len, sizeof(size_t));
        off += sizeof(size_t);

        std::memcpy(buf.get() + off, &page.description_len, sizeof(size_t));
        off += sizeof(size_t);
    }

    bool result = send_exact(fd, buf.get(), total);  
    return result;
}

// serializes the lean page results and sends it back to client
inline bool send_client_response(const uint16_t fd, vector<LeanPage> results) {

}