#pragma once

#include "../lib/rpc_listener.h"
#include "../lib/rpc_query_handler.h"
#include "../lib/consts.h"
#include "../url_store/url_store.h"
#include "../lib/thread_pool.h"

#include <thread>

// receives requests for word doc data from another machine's query handlers
class IndexServer {
    private:
        // TODO(charlie): presize isr_map so it doesnt blow up memory
        unordered_map<string, IndexStreamReader*> isr_map; // map from word to the corresponding ISR for that word on this machine --- TODO: need to determine how we are going to initialize and populate this map
        vector<LoadedIndex> indexChunks;
        RPCListener* rpc_listener;      // Listener for client requests
        UrlStore* url_store;            // For looking up url info to include in the response
        std::thread listener_thread;    // Thread running the listener loop
        ThreadPool word_pool;           // thread pool to concurrently handle multiple word queries at once
        ThreadPool index_chunk_pool;    // thread pool to query multiple index chunks for a given word query at once

        RankedPageResponse handle_request(const string& word, bool is_phrase) {
            // TODO(charlie): handle is_Phrase logic and break down word into multi-words and enforce ordering

            RankedPageResponse results;
            // TODO(charlie): query index for matching docs/urls and the pos of the word given the word

            // leverage thread pool to query multiple index chunks at the same time
            vector<string> urls;
            vector<vector<size_t>> word_positions;

            for (const string& url : urls) {
                RankedPage page;
                page.url = string(url.data(), url.size());
                auto data = url_store->getUrl(url);
                if (data) {
                    page.title = string(data->title.data(), data->title.size());
                    page.seed_list_dist = data->seed_distance;
                    page.domains_from_seed = data->domain_dist;
                    page.num_unique_words_found_anchor = data->anchor_freqs.size();

                    // TODO(charlie): for now not going to calculate here, and might leave to ranker to calculate and populate these fields
                    // int num_unique_words_found_title;
                    // int num_unique_words_found_descr;
                    // int num_unique_words_found_url;
                    page.doc_len = data->eod;
                    page.times_seen = data->num_encountered;
                    results.pages.push_back(std::move(page));
                }
                
                // populate word_positions
            }

            return results;
        }

        void client_handler(int fd) {
            std::optional<string> word_opt = recv_string(fd);
            if (!word_opt) {
                close(fd);
                return;
            }

            word_pool.enqueue_task([this, fd, word = std::move(word_opt.value())]() {
                RankedPageResponse results = handle_request(word);
                send_word_response(fd, results);
                close(fd);
            });
            
            
        }

    public:
        IndexServer(UrlStore* url_store, const vector<LoadedIndex>& indexChunks) : url_store(url_store), indexChunks(indexChunks) {
            rpc_listener = new RPCListener(INDEX_SERVER_PORT, INDEX_SERVER_NUM_THREADS);
            listener_thread = std::thread([this]() {
                rpc_listener->listener_loop([this](int fd) { client_handler(fd); });
            });
        }
        ~IndexServer() {
            for (auto isr_it = isr_map.begin(); isr_it != isr_map.end(); ++isr_it) {
                delete (*isr_it).value;
            }
        }

        // some internal method that this machine's query handler can traverse this index without needing to make a network call
        std::future<vector<RankedPage>> local_retrieve(const string_view& wordview, bool is_phrase) {
            auto promise = std::make_shared<std::promise<vector<RankedPage>>>();
            std::future<vector<RankedPage>> future = promise->get_future();
            
            word_pool.enqueue_task([this, wordview, is_phrase, promise]() {
                try {
                    RankedPageResponse response = this->handle_request(wordview.to_string(), is_phrase);
                    promise->set_value(std::move(response.pages));

                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });

            return future;
        }
};