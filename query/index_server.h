#pragma once

#include "../lib/rpc_listener.h"
#include "../lib/rpc_query_handler.h"
#include "../lib/consts.h"
#include "../url_store/url_store.h"
#include "../lib/thread_pool.h"
#include "../ranker/Ranker.h"
#include "../lib/query_response.h"
#include "../index_manager/index_manager.h"

#include <thread>

// receives requests for word doc data from another machine's query handlers
class IndexServer {
    private:
        IndexManager* index_manager;
        RPCListener* rpc_listener;      // Listener for client requests
        UrlStore* url_store;            // For looking up url info to include in the response
        std::thread listener_thread;    // Thread running the listener loop
        ThreadPool query_pool;           // thread pool to concurrently handle multiple word queries at once
        Ranker* ranker;

        LeanPageResponse handle_request(const string& query) {


            LeanPageResponse results;
            vector<RankedPage> candidates;
            vector<RankerNodeInfo> ranker_info;
            // TODO(charlie): call index_manager to query index chunks and get QueryResponse
            QueryResponse qr;
            // iterate through query response entries urls and retrieve appropriate info from urlStore
                // construct rankedPages for each docInfo from queryResponse and send THIS back to client as RankedPageResponse obj.
            for (const DocInfo& di : qr.pages) {
                const string& url = di.url;
                const vector<NodeInfo>& phrases = di.nodeInfo;
                RankedPage page;
                page.url = string(url.data(), url.size());
                auto data = url_store->getUrl(url);
                if (data) {
                    page.title = string(data->title.data(), data->title.size());
                    page.seed_list_dist = data->seed_distance;
                    page.domains_from_seed = data->domain_dist;
                    page.num_unique_words_found_anchor = data->anchor_freqs.size();

                    // Work to calculate:
                    // int num_unique_words_found_title;
                    // int num_unique_words_found_url;
                    for (const NodeInfo& ni : phrases) {
                        const string& phrase = ni.phrase;
                        
                    }
                    
                    page.doc_len = data->eod;
                    page.times_seen = data->num_encountered; 

                    // TODO: populate word_positions


                    candidates.push_back(std::move(page));;
                }
            }

            ranker->set_new_query(ranker_info);
            results.pages = r->rank(candidates);
            ranker->reset();
            return results;
        }

        void client_handler(int fd) {
            // also needs to receive is_phrase bool on top of word string value
            std::optional<string> query_opt = recv_string(fd);
            if (!query_opt) {
                close(fd);
                return;
            }

            query_pool.enqueue_task([this, fd, query = std::move(query_opt.value())]() {
                LeanPageResponse results = handle_request(query);
                send_query_response(fd, results);
                close(fd);
            });  
        }

    public:
        IndexServer(UrlStore* url_store, IndexManager* index_manager, Ranker* ranker) : url_store(url_store), query_pool(INDEX_SERVER_NUM_THREADS), index_manager(index_manager), ranker(ranker) {
            rpc_listener = new RPCListener(INDEX_SERVER_PORT, INDEX_SERVER_NUM_THREADS);
            listener_thread = std::thread([this]() {
                rpc_listener->listener_loop([this](int fd) { client_handler(fd); });
            });
        }

        // some internal method that this machine's query handler can traverse this index without needing to make a network call
        std::future<LeanPageResponse> local_retrieve(const string& query) {
            auto promise = std::make_shared<std::promise<LeanPageResponse>>();
            std::future<LeanPageResponse> future = promise->get_future();
            
            query_pool.enqueue_task([this, new_query = string(query.data(), query.size()), promise]() {
                try {
                    LeanPageResponse response = this->handle_request(new_query);
                    promise->set_value(std::move(response));
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });

            return future;
        }
};