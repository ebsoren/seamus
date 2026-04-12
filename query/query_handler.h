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
        IndexStreamReader* isr;    // local ISR to query for words whose index chunks are on this machine
        ThreadPool pool;

    public:
        QueryHandler(IndexStreamReader* isr, size_t n_threads = NUM_MACHINES) : pool(n_threads), isr(isr) { };

        // TODO: determine final input semantics for get_results
        vector<RankedPage> get_results(vector<string> query_words) {

            // return doc ids and positions of query words in those docs for ranking to use
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
            for (const string& word : query_words) {
                for (size_t i = 0; i < NUM_MACHINES; ++i) {
                    auto task_promise = std::make_shared<std::promise<vector<RankedPage>>>();
                    futures.push_back(task_promise->get_future());

                    // TODO: might need to make copy of word otherwise, original word value might get deleted before the task runs and we try to access it in the task
                    pool.enqueue_task([this, &word, i, task_promise]() {
                        vector<RankedPage> local_hits;
                        
                        // ... Send RPC request to Machine 'i' for 'word' ...
                        if (i == my_machine_id()) {
                            local_hits = this->isr->advance_to_doc(123); // TODO: need to determine how to use the ISR to get the relevant info for the word on this machine
                        } else {
                            local_hits = send_recv_word_data(string(get_machine_addr(i)), INDEX_SERVER_PORT, word); // TODO: need to implement this function to send the RPC request to machine i and parse the response into a vector of RankedPages
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
                
                // TODO(charlie): given these machine_hits, successfully merge into final_results existing rankedPages
            }

            // TODO(charlie): after all results are created, determine if any RankedPage results should just be pruned altogether to save ranker work
            vector<RankedPage> ranked_pages;
            for (auto results_it = final_results.begin(); results_it != final_results.end(); ++results_it) {
                ranked_pages.push_back(std::move((*results_it).value));
            }

            return ranked_pages;
        }

};