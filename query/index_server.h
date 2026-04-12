#pragma once

#include "../lib/rpc_listener.h"
#include "../lib/rpc_query_handler.h"
#include "../lib/consts.h"

#include <thread>

// receives requests for word doc data from another machine's query handlers
class IndexServer {
    private:
        RPCListener* rpc_listener;      // Listener for client requests
        std::thread listener_thread;    // Thread running the listener loop
        void client_handler(int fd) {
            std::optional<string> word_opt = recv_string(fd);
            if (!word_opt) {
                close(fd);
                return;
            }

            RankedPageResponse results;
            // TODO(charlie): fetch doc results here
            
            send_word_response(fd, results);
            close(fd);
        }

    public:
        IndexServer() {
            rpc_listener = new RPCListener(INDEX_SERVER_PORT, INDEX_SERVER_NUM_THREADS);
            listener_thread = std::thread([this]() {
                rpc_listener->listener_loop([this](int fd) { client_handler(fd); });
            });
        }
};