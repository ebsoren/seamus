#pragma once

#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/unordered_map.h"
#include "../lib/utils.h"
#include "../lib/consts.h"
#include "../lib/rpc_listener.h"
#include "../lib/rpc_query_handler.h"
#include "../index-stream-reader/isr.h"
#include "index_server.h"

#include <thread>

#include "../ranker/Ranker.h"
#include <future>

class QueryHandler {
    private:
        Ranker *r;
        ThreadPool pool;
        RPCListener rpc_listener;
        IndexServer *index_server;

        vector<RankedPage> get_results(Result query_combinations) {
            // TODO(charlie): after all results are created, determine if any RankedPage results should just be pruned altogether to save ranker work
            vector<RankedPage> ranked_pages;
            for (const Clause& clause : query_combinations) {
                vector<std::future<vector<RankedPage>>> futures;
                // takes words and starts with rarest first

                // for each word
                    // fan out requests to all machines to retrieve a total list of all docs that word appears in
                    // response needs to include enough information for each page to create a RankedPage
                        // this each machine needs to:
                            // given a word, retrieve list of docIDs and word positions
                            // find the corresponding url to that docID and query the urlStore for url info
                            // batch all of this data back in the response sent back to this queryHandler
                        // as we get responses about documents for a word, merge them into the ongoing set of results we are storing and return
                for (auto& word : clause) {
                    for (size_t i = 0; i < NUM_MACHINES; ++i) {
                        auto task_promise = std::make_shared<std::promise<vector<RankedPage>>>();
                        futures.push_back(task_promise->get_future());

                        // TODO: might need to make copy of word otherwise, original word value might get deleted before the task runs and we try to access it in the task
                        pool.enqueue_task([this, &word, i, task_promise]() {
                            vector<RankedPage> local_hits;
                            
                            // ... Send RPC request to Machine 'i' for 'word' ...
                            if (i == my_machine_id()) {
                                local_hits = index_server->local_retrieve(word.token);
                            } else {
                                local_hits = send_recv_word_data(string(get_machine_addr(i)), INDEX_SERVER_PORT, word.token); // TODO: need to implement this function to send the RPC request to machine i and parse the response into a vector of RankedPages
                            }
                            
                            task_promise->set_value(std::move(local_hits));
                        });
                    }
                }

                // map from docID to rankedPage data
                unordered_map<uint32_t, RankedPage> final_results;
                for (auto& fut : futures) {
                    // .get() will BLOCK the main thread here until this specific background task 
                    // calls set_value() on its promise.
                    vector<RankedPage> machine_hits = fut.get(); 
                    
                    // TODO(charlie): given these machine_hits, merge into final_results
                }

                // merge final_results into ranked_pages
                for (auto results_it = final_results.begin(); results_it != final_results.end(); ++results_it) {
                    ranked_pages.push_back(std::move((*results_it).value));
                }
            }

            return ranked_pages;
        }

        Result compile_query(const string& raw_request) {

        }


        bool handle_client_req(int client_fd) {
            char buffer[2048] = {0};
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            
            if (bytes_read <= 0) {
                close(client_fd);
                return;
            }

            string raw_request(buffer);
            vector<RankedPage> results = get_results(compile_query(raw_request));

            // pass results off to ranker
            r->rank(results);
            send_client_response(client_fd, r->get_top_x(NUM_RESULTS_RETURN)); // TODO: determine how many results we want to return
            r->reset(); // flush ranker for future queries
        }

    public:
        QueryHandler(Ranker* r, IndexServer* index_server, uint16_t port = QUERY_HANDLER_PORT, size_t n_pool_threads = NUM_MACHINES, size_t n_query_threads = QUERY_NUM_LISTENING_THREADS) : pool(n_pool_threads), r(r), index_server(index_server), rpc_listener(port, n_query_threads) { 

        };

        void start() {
            rpc_listener.listener_loop([this](int client_fd) {
                this->handle_client_req(client_fd);
            });
        }

};