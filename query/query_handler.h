#pragma once

#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/unordered_map.h"
#include "../lib/utils.h"
#include "../lib/consts.h"
#include "../lib/rpc_listener.h"
#include "../lib/rpc_query_handler.h"
#include "../lib/thread_pool.h"

#include "../index-chunk/chunk-manager.h"
#include "index_server.h"
#include "../ranker/Ranker.h"
#include "query_helpers.h"

#include <thread>
#include <future>

class QueryHandler {
    private:
        Ranker *r;
        ThreadPool pool;
        RPCListener rpc_listener;
        IndexServer *index_server;

        vector<RankedPage> get_results(const string& input) {
            ParseResult result = parse_query_tree(input.str_view(0, input.size()));
            vector<std::future<vector<QueryIndexResult>>> futures;
            for (UniqueTerm term: result.unique_terms) {
                // if is_phrase == true, then string_view val is multiwords space delimited
                // if "the quick brown" then itll be one term w/o quotes, there will still be a tag and it can be phrase
                for (size_t i = 0; i < NUM_MACHINES; ++i) {
                    auto task_promise = std::make_shared<std::promise<vector<QueryIndexResult>>>();
                    futures.push_back(task_promise->get_future());
                    string_view safe_view;
                    bool is_phrase = false;
                    if (term.is_phrase) {
                        is_phrase = true;
                    } else {
                        safe_view = string_view(term.val.data(), term.val.size());
                    }

                    
                    pool.enqueue_task([this, word_token = std::move(safe_view), is_phrase, i, task_promise]() {
                        vector<QueryIndexResult> local_hits;
                        
                        if (i == my_machine_id()) {
                            local_hits = index_server->local_retrieve(word_token, is_phrase).get();
                        } else {
                            // send is_phrase
                            local_hits = send_recv_word_data(string(get_machine_addr(i)), INDEX_SERVER_PORT, word_token, is_phrase); // TODO: need to implement this function to send the RPC request to machine i and parse the response into a vector of RankedPages
                        }
                        
                        task_promise->set_value(std::move(local_hits));
                    });
                }
            }

            // map from url to rankedPage data
            unordered_map<string, RankedPage> final_results;
            for (auto& fut : futures) {
                // .get() will BLOCK the main thread here until this specific background task 
                vector<QueryIndexResult> machine_hits = fut.get(); 
                
                // TODO(charlie): given these machine_hits, merge into final_results
                for (QueryIndexResult& qir : machine_hits) {
                    RankedPage& rp = qir.rp;
                    const string& word = qir.word;
                    RankedPage* curr_result = &final_results[rp.url];
                    curr_result->url = std::move(rp.url);
                    curr_result->title = std::move(rp.title);
                    curr_result->seed_list_dist = rp.seed_list_dist;
                    
                    curr_result->num_unique_words_found_anchor += rp.num_unique_words_found_anchor;
                    curr_result->num_unique_words_found_title += rp.num_unique_words_found_title;
                    curr_result->num_unique_words_found_url += rp.num_unique_words_found_url;
                    curr_result->times_seen = rp.times_seen;
                }
            }
            
            vector<RankedPage> ranked_pages;
            // merge final_results into ranked_pages
            for (auto results_it = final_results.begin(); results_it != final_results.end(); ++results_it) {
                // TODO(charlie): for each result, validate and make sure that all AND terms are included in the 2D positions vector
                bool valid_query = evaluate_query(result.root, postns);
                if (!valid_query) continue;
                // ignore results that have empty vectors for AND terms
                ranked_pages.push_back(std::move((*results_it).value));
            }

            return ranked_pages;
        }

        bool handle_client_req(int client_fd) {
            char buffer[2048] = {0};
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
            
            if (bytes_read <= 0) {
                close(client_fd);
                return;
            }

            string raw_request(buffer);
            vector<RankedPage> results = get_results(raw_request);

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