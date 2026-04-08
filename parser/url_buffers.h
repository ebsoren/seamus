#pragma once

#include <cstdint>
#include <cstdlib>
#include <mutex>
#include "lib/consts.h"
#include "word_array.h"
#include "../lib/logger.h"
#include "../lib/rpc_urlstore.h"
#include "../lib/string.h"
#include "../lib/vector.h"
#include "../url_store/url_store.h"

// Rename this to parser's local url buffer or something
// Add global data that mimics that of crawler outbound (buffer, locks for each slot)
// Bring crawler outbound logic here
// We'll lock around the URL inclusion loop and push to the buffer
class OutboundUrlBuffer {
public:
    OutboundUrlBuffer() : ME(-1), urlStore(nullptr) {
        for (size_t i = 0; i < NUM_MACHINES; i++) {
            buffers[i] = vector<URLStoreUpdateRequest>();
        }
    }

    OutboundUrlBuffer(size_t machine_id, UrlStore* local_store)
        : ME(machine_id), urlStore(local_store) {
        for (size_t i = 0; i < NUM_MACHINES; i++) {
            buffers[i] = vector<URLStoreUpdateRequest>();
        }
    }

    // Invariant: lock must be called by the LOCAL buffer outside of add_url being invoked
    void add_url(size_t machine_id, URLStoreUpdateRequest&& target) {
        buffers[machine_id].push_back(move(target));
        if (buffers[machine_id].size() >= CRAWLER_OUTBOUND_BATCH_SIZE) {
            // logger::info("pushing urls to machine %zd", machine_id);
            flush(machine_id);
        }
    }

    friend class LocalUrlBuffer;
private:
    vector<URLStoreUpdateRequest> buffers[NUM_MACHINES];
    std::mutex locks[NUM_MACHINES];
    size_t ME;
    UrlStore* urlStore;

    void flush(size_t machine_id) {
        BatchURLStoreUpdateRequest batch;
        batch.reqs = move(buffers[machine_id]);
        buffers[machine_id] = vector<URLStoreUpdateRequest>();

        if (machine_id == ME && urlStore) {
            urlStore->batch_manage_frontier_and_update_url(batch);
        } else {
            std::thread(&OutboundUrlBuffer::flush_async, this, machine_id, std::move(batch)).detach();
        }
    }

    void flush_async(size_t machine_id, BatchURLStoreUpdateRequest batch){
        const char* addr = get_machine_addr(machine_id);
        string host(addr, strlen(addr));
        if (!send_batch_urlstore_update(host, URL_STORE_PORT, batch)) {
            logger::warn("Failed to send outbound batch to machine %zu (%s)", machine_id, addr);
        }
    }
};

class LocalUrlBuffer {
public:
    LocalUrlBuffer() : ME(-1) {}

    LocalUrlBuffer(size_t machine_id, OutboundUrlBuffer* outbound_buff)
    : outboundBuffer(outbound_buff), ME(machine_id) {}

    bool add_urls(buffer_array<MAX_LINK_MEMORY> &urls) {
        /**
         * URL array should have a list of docs of format:
         *
         * <doc>
         * [old hops] [old domain hops]
         * [domain]
         * [
         *      list of links in format:
         *      [link]
         *      [anchor text word 1] ... [anchor text word n]
         * ]
         * </doc>
        */
        if (ME == -1) {
            logger::error("Local URL buffer not initialized! add_urls failing.");
            return false;
        }

        const char* p = urls.data();
        const char* const end = p + urls.size();
        bool ok = true;
        logger::debug("add_urls: parsing %zu bytes of link data", urls.size());

        while (p < end) {
            uint16_t hops = 0;
            uint16_t domain_hops = 0;

            // Expect start of document token
            if (p + 5 > end || memcmp(p, "<doc>", 5) != 0) {
                logger::warn("add_urls: missing <doc> token at offset %zu (remaining=%zu)",
                             (size_t)(p - urls.data()), (size_t)(end - p));
                ok = false;
                break;
            }

            p += 6; // Skip <doc>\n
            if (p >= end) {
                logger::warn("add_urls: truncated data after <doc> tag");
                ok = false;
                break;
            }

            // Read number (TODO: Test this use of strtol)
            char* next;
            hops = strtol(p, &next, 10);
            domain_hops = strtol(next, &next, 10);

            // Advance p to point past the nums and the new line
            p = next + 1;
            if (p >= end) {
                logger::warn("add_urls: truncated data after hops fields");
                ok = false;
                break;
            }

            // Read the domain of the page the URLs were found on
            const char* word_start = p;
            while (p < end && *p != '\n') p++;
            if (p >= end) {
                logger::warn("add_urls: truncated data while reading domain");
                ok = false;
                break;
            }
            string domain = string(word_start, p - word_start);
            p++;

            // Parse URLs until </doc> is reached
            while (p + 6 <= end && memcmp(p, "</doc>", 6) != 0) {
                // Skip blank lines (can appear when flush terminates a partial line)
                if (*p == '\n') { p++; continue; }
                // Get the URL itself
                word_start = p;
                while (p < end && *p != '\n') p++;
                if (p >= end) {
                    logger::warn("add_urls: truncated data while reading URL");
                    ok = false;
                    break;
                }

                if (p - word_start == 6 && memcmp(word_start, "</doc>", 6) == 0) {
                    p = word_start; // rewind so the outer loop sees </doc>
                    break;
                }
                string url(word_start, p - word_start);
                uint16_t dhop = extract_domain(url) != domain ? 1 : 0;
                vector<string> anchor_words;

                // Parse space-separated anchor texts
                p++;
                while (p < end && *p != '\n') {
                    word_start = p;
                    while (p < end && *p != ' ' && *p != '\n') p++;
                    if (p > word_start) {
                        anchor_words.push_back(string(word_start, p - word_start));
                    }
                    if (p < end && *p == ' ') p++;
                }
                if (p >= end) {

                    logger::warn("add_urls: truncated data while reading anchor text");
                    ok = false;
                    break;
                }
                p++; // skip the '\n' after anchor text line

                size_t recipient = get_destination_machine_from_url(url);

                URLStoreUpdateRequest rpc{
                    move(url),
                    move(anchor_words),
                    1,
                    static_cast<uint16_t>(hops + 1),
                    static_cast<uint16_t>(domain_hops + dhop),
                };

                url_updates[recipient].push_back(move(rpc));
            }

            if (!ok) break;

            if (p + 6 > end) {
                if (p >= urls.data() + 7 && memcmp(p - 7, "</doc>\n", 7) == 0)
                    break;
                logger::warn("add_urls: truncated data, missing </doc> tag");
                ok = false;
                break;
            }

            p += 7; // Skip </doc>\n
        }

        // Always dispatch parsed URLs — even on truncation, the complete
        // entries before the bad data are valid and must be sent
        pass_rpcs();
        return ok;
    }

private:
    vector<URLStoreUpdateRequest> url_updates[NUM_MACHINES];
    OutboundUrlBuffer* outboundBuffer;
    size_t ME;

    // Once all URLs have been parsed into update requests, pass to outbound buffer
    void pass_rpcs() {
        for (size_t i = 0; i < NUM_MACHINES; i++) {
            if (url_updates[i].empty()) continue;
            vector<URLStoreUpdateRequest> batch = std::move(url_updates[i]);
            url_updates[i] = vector<URLStoreUpdateRequest>();
            std::lock_guard<std::mutex> lock(outboundBuffer->locks[i]);
            for (URLStoreUpdateRequest& req : batch) {
                outboundBuffer->add_url(i, std::move(req));
            }
        }
    }
};
