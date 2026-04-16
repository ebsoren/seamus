#pragma once

#include "../lib/rpc_listener.h"
#include "../lib/rpc_query_handler.h"
#include "../lib/consts.h"
#include "../lib/thread_pool.h"
#include "../index_manager/index_manager.h"

#include <future>
#include <thread>

// receives requests for word doc data from another machine's query handlers
class IndexServer {
    private:
        IndexManager* index_manager;
        RPCListener* rpc_listener;      // Listener for client requests
        std::thread listener_thread;    // Thread running the listener loop
        ThreadPool query_pool;           // thread pool to concurrently handle multiple word queries at once

        LeanPageResponse handle_request(const string& query) {
            return index_manager->handle_query(query);
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
        IndexServer(IndexManager* index_manager) : query_pool(INDEX_SERVER_NUM_THREADS), index_manager(index_manager), ranker(ranker) {
            rpc_listener = new RPCListener(INDEX_SERVER_PORT, INDEX_SERVER_NUM_THREADS);
            listener_thread = std::thread([this]() {
                rpc_listener->listener_loop([this](int fd) { client_handler(fd); });
            });
        }

        ~IndexServer() {
            delete rpc_listener;
            if (listener_thread.joinable()) {
                listener_thread.join();
            }
        }

        // some internal method that this machine's query handler can traverse this index without needing to make a network call
        std::future<LeanPageResponse> local_retrieve(const string& query) {
            auto promise = std::make_shared<std::promise<LeanPageResponse>>();
            std::future<LeanPageResponse> future = promise->get_future();
            
            string query_copy = string(query.data(), query.size());
            query_pool.enqueue_task([this, new_query = std::move(query_copy), promise]() {
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