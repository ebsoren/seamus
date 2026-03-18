#pragma once

#include <cstdint>
#include <cstdlib>
#include <mutex>
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
    OutboundUrlBuffer() {
        for (size_t i = 0; i < NUM_MACHINES; i++) {
            buffers[i] = vector<URLStoreUpdateRequest>();
        }
    }

    // Invariant: lock must be called by the LOCAL buffer outside of add_url being invoked 
    void add_url(size_t machine_id, URLStoreUpdateRequest&& target) {
        buffers[machine_id].push_back(move(target));
        if (buffers[machine_id].size() >= CRAWLER_OUTBOUND_BATCH_SIZE) {
            flush(machine_id);
        }
    }

    friend class LocalUrlBuffer;
private:
    vector<URLStoreUpdateRequest> buffers[NUM_MACHINES];
    std::mutex locks[NUM_MACHINES];

    void flush(size_t machine_id) {
        BatchURLStoreUpdateRequest batch;
        batch.reqs = move(buffers[machine_id]);
        buffers[machine_id] = vector<URLStoreUpdateRequest>();

        // Send RPC in a separate thread so we can return & release the lock ASAP
        // Safe to release the lock bc we copied and reset the buffer
        std::thread(&OutboundUrlBuffer::flush_async, this, machine_id, std::ref(batch)).detach();
    }

    // Send the copied contents of an RPC buffer to the correct machine
    void flush_async(size_t machine_id, BatchURLStoreUpdateRequest& batch){
        const char* addr = get_machine_addr(machine_id);
        string host(addr, strlen(addr));
        if (!send_batch_urlstore_update(host, CRAWLER_LISTENER_PORT, batch)) {
            logger::warn("Failed to send outbound batch to machine %zu (%s)", machine_id, addr);
        }
    }
};

class LocalUrlBuffer {
public:
    LocalUrlBuffer() : ME(-1) {}
    
    LocalUrlBuffer(size_t machine_id, OutboundUrlBuffer* outbound_buff, UrlStore* local_store)
    : urlStore(local_store), outboundBuffer(outbound_buff), ME(machine_id) {}

    bool add_urls(word_array<MAX_LINK_MEMORY> &urls) {
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

        while (p < end) {
            uint16_t hops = 0;
            uint16_t domain_hops = 0;

            // Expect start of document token
            if (memcmp(p, "<doc>", 5)!=0) {
                // Didn't find expected token
                logger::warn("Missing expected start of document token.");
                return false;
            }

            p += 6; // Skip <doc>\n

            // Read number (TODO: Test this use of strtol)
            char* next;
            hops = strtol(p, &next, 10);
            domain_hops = strtol(next, &next, 10);

            // Advance p to point past the nums and the new line
            p = next + 1;

            // Read the domain of the page the URLs were found on
            const char* word_start = p;
            while (*(++p) != '\n') {}
            string domain = string(word_start, p - word_start);
            p++;

            // Parse URLs until </doc> is reached
            while (memcmp(p, "</doc>", 6) != 0) {
                // Get the URL itself
                word_start = p;
                while(*(++p) != '\n') {}
                string url(word_start, p - word_start);
                uint16_t dhop = extract_domain(url) != domain ? 1 : 0;
                vector<string> anchor_words;

                // Parse space-separated anchor texts
                while (*(++p) != '\n') {
                    word_start = p;
                    while(*(++p) != ' ') {}
                    anchor_words.push_back(string(word_start, p - word_start));
                }

                size_t recipient = get_destination_machine_from_url(url);
                
                // URL belongs to this machine
                if (recipient == ME) {
                    urlStore->updateUrl(url, anchor_words, hops + 1, domain_hops + dhop, 1);
                } 
                // Create RPC for destination machine
                else {
                    URLStoreUpdateRequest rpc{
                        move(url), // Can move here bc url is discarded after this call
                        move(anchor_words), 
                        1,
                        static_cast<uint16_t>(hops + 1),
                        static_cast<uint16_t>(domain_hops + dhop),
                    };

                    url_updates[recipient].push_back(move(rpc));
                }
            }

            p += 7; // Skip </doc>\n
        }

        pass_rpcs();
        return true;
    }

private:
    vector<URLStoreUpdateRequest> url_updates[NUM_MACHINES];
    UrlStore* urlStore;
    OutboundUrlBuffer* outboundBuffer;
    size_t ME;

    // Once all URLs have been parsed into update requests, pass to outbound buffer
    void pass_rpcs() {
        for (size_t i = 0; i < NUM_MACHINES; i++) {
            std::thread(&LocalUrlBuffer::pass_rpc, this, i).detach();
        }
    }

    // Pass a single machine's RPCs to the outbound buffer
    void pass_rpc(size_t id) {
        outboundBuffer->locks[id].lock();
        for (URLStoreUpdateRequest& req : url_updates[id]) {
            outboundBuffer->add_url(id, move(req));
        }
        outboundBuffer->locks[id].unlock();
    }
};