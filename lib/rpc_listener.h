#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <cstddef>
#include <functional>
#include "thread_pool.h"


class RPCListener {
public:

    // Constructor: creates and binds a listening socket on the given port
    RPCListener(uint16_t port, size_t n_threads) : listen_fd(-1), pool(n_threads) {
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) return;

        int opt = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(listen_fd);
            listen_fd = -1;
            return;
        }

        if (listen(listen_fd, SOMAXCONN) < 0) {
            close(listen_fd);
            listen_fd = -1;
        }
    }


    // Destructor: close the listening socket
    ~RPCListener() {
        if (listen_fd >= 0) close(listen_fd);
    }


    // Listener loop: blocks forever, calling handler with each new client fd.
    // The handler is responsible for reading/writing to the fd and closing it when done.
    void listener_loop(std::function<void(int)> handler) {
        while (true) {
            struct sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) continue;

            // Enqueue ephemeral socket handler as a task to the thread pool
            pool.enqueue_task([handler, client_fd]{ handler(client_fd); });
        }
    }


    // Interface: check for listen socket validity before calling the accept loop
    bool valid() const { return listen_fd >= 0; }


private:
    int listen_fd;
    ThreadPool pool;
};
