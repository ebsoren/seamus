#pragma once

#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/unordered_map.h"
#include "../lib/utils.h"
#include "../lib/consts.h"
#include "../lib/rpc_query_handler.h"
#include "../lib/thread_pool.h"
#include "../lib/algorithm.h"

#include "index_server.h"
#include "query_helpers.h"

#include <thread>
#include <future>


//chunk manager receives entire raw string query
// builds tree and each chunk pulls out matching docs for primiate queriies or phrase queries
// then we have to merge somewhere else for OR and AND opertors
    // - can have the end machine od this then send this over back to the request machine
    // or can send these resutls and have the request machine merge
class QueryHandler {
    private:
        ThreadPool pool;
        IndexServer *index_server;

        vector<LeanPage> get_results(string& input) {
            ParseResult result = parse_query_tree(input.str_view(0, input.size()));
            vector<std::future<vector<LeanPage>>> futures;

            // - batch the WHOLE string query to each machine instead of 18 rpcs per word
                // each end machine (index server) should pass this raw string to indexManager, and indexManager should construct the tree
            // if is_phrase == true, then string_view val is multiwords space delimited
            // if "the quick brown" then itll be one term w/o quotes, there will still be a tag and it can be phrase
            for (size_t i = 0; i < NUM_MACHINES; ++i) {
                auto task_promise = std::make_shared<std::promise<vector<LeanPage>>>();
                futures.push_back(task_promise->get_future());
                string input_copy = string(input.data(), input.size());
                pool.enqueue_task([this, query = std::move(input_copy), i, task_promise]() {
                    try {
                        vector<LeanPage> local_hits;
                        if (i == my_machine_id()) {
                            local_hits = index_server->local_retrieve(query).get().pages;
                        } else {
                            local_hits = send_recv_query_data(string(get_machine_addr(i)), INDEX_SERVER_PORT, query).pages; 
                        }
                        task_promise->set_value(std::move(local_hits));
                        
                    } catch (...) {
                        // If the network fails, gracefully return 0 hits for this specific machine
                        // instead of crashing the whole Query Handler!
                        task_promise->set_value(vector<LeanPage>());
                    }
                });
            }

            // map from url to rankedPage data
            vector<LeanPage> final_results;
            for (auto& fut : futures) {
                // .get() will BLOCK the main thread here until this specific background task 
                vector<LeanPage> machine_hits = fut.get(); 
                for (LeanPage& lp : machine_hits) final_results.push_back(std::move(lp));
            }

            // merge sort these final_results and return final 10
            sort<LeanPage>(final_results, [](const LeanPage& a, const LeanPage& b) {
                    return a.score > b.score; 
                }
            );

            if (final_results.size() > NUM_RESULTS_RETURN) {
                final_results.resize(NUM_RESULTS_RETURN);
            }

            return final_results;
        }

        

    public:
        QueryHandler(IndexServer* index_server, uint16_t port = QUERY_HANDLER_PORT, size_t n_threads = NUM_MACHINES) : pool(n_threads), index_server(index_server) { };

        vector<LeanPage> handle_client_req(string& query_str) {
            return get_results(query_str);
        }

};