#pragma once

#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/unordered_map.h"
#include "../lib/utils.h"
#include "../lib/consts.h"
#include "../lib/rpc_listener.h"
#include "../lib/rpc_query_handler.h"
#include "../lib/thread_pool.h"

#include "../index_chunk/chunk_manager.h"
#include "index_server.h"
#include "../ranker/Ranker.h"
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
        Ranker *r;
        ThreadPool pool;
        RPCListener rpc_listener;
        IndexServer *index_server;

        vector<RankedPage> get_results(string& input) {
            ParseResult result = parse_query_tree(input.str_view(0, input.size()));
            vector<std::future<vector<RankedPage>>> futures;

                        
            // TODO:
                // - batch the WHOLE string query to each machine instead of 18 rpcs per word
                    // each end machine (index server) should pass this raw string to indexManager, and indexManager should construct the tree
            // if is_phrase == true, then string_view val is multiwords space delimited
            // if "the quick brown" then itll be one term w/o quotes, there will still be a tag and it can be phrase
            for (size_t i = 0; i < NUM_MACHINES; ++i) {
                auto task_promise = std::make_shared<std::promise<vector<RankedPage>>>();
                futures.push_back(task_promise->get_future());
                
                pool.enqueue_task([this, query = std::move(input), i, task_promise]() {
                    vector<RankedPage> local_hits;
                    
                    if (i == my_machine_id()) {
                        local_hits = index_server->local_retrieve(query).get().pages;
                    } else {
                        // send is_phrase
                        local_hits = send_recv_query_data(string(get_machine_addr(i)), INDEX_SERVER_PORT, query).pages; // TODO: need to implement this function to send the RPC request to machine i and parse the response into a vector of RankedPages
                    }
                    
                    task_promise->set_value(std::move(local_hits));
                });
            }

            // map from url to rankedPage data
            vector<RankedPage> final_results;
            for (auto& fut : futures) {
                // .get() will BLOCK the main thread here until this specific background task 
                vector<RankedPage> machine_hits = fut.get(); 
                for (const RankedPage& rp : machine_hits) final_results.push_back(std::move(rp));
            }

            return final_results;
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
            r->rank(results);
            send_client_response(client_fd, r->get_top_x(NUM_RESULTS_RETURN)); // TODO: determine how many results we want to return
            r->reset(); // flush ranker for future queries
        }

    public:
        QueryHandler(Ranker* r, IndexServer* index_server, uint16_t port = QUERY_HANDLER_PORT, size_t n_pool_threads = NUM_MACHINES, size_t n_query_threads = QUERY_NUM_LISTENING_THREADS) : pool(n_pool_threads), r(r), index_server(index_server), rpc_listener(port, n_query_threads) { };

        void start() {
            rpc_listener.listener_loop([this](int client_fd) {
                this->handle_client_req(client_fd);
            });
        }

};