#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <atomic>
#include "thread_pool.h"


class RPCListener {
public:

    // Constructor: creates and binds a listening socket on the given port
    RPCListener(uint16_t port, size_t n_threads) : listen_fd{-1}, pool(n_threads) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return;

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(fd);
            return;
        }

        if (listen(fd, SOMAXCONN) < 0) {
            close(fd);
            return;
        }

        listen_fd.store(fd);
    }


    // Destructor: stop the listener and close the listening socket
    ~RPCListener() {
        stop();
    }


    // Listener loop: blocks until stop() is called, calling handler with each new client fd.
    // The handler is responsible for reading/writing to the fd and closing it when done.
    void listener_loop(std::function<void(int)> handler) {
        while (!stopped) {
            struct sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            int fd = listen_fd.load();
            if (fd < 0) break;
            int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) continue;

            // Enqueue ephemeral socket handler as a task to the thread pool
            pool.enqueue_task([handler, client_fd]{ handler(client_fd); });
        }
    }


    // Stop the listener loop by closing the listen fd so accept() returns -1
    void stop() {
        stopped = true;
        int fd = listen_fd.exchange(-1);
        if (fd >= 0) close(fd);
    }


    // Interface: check for listen socket validity before calling the accept loop
    bool valid() const { return listen_fd.load() >= 0; }


private:
    std::atomic<int> listen_fd;
    std::atomic<bool> stopped = false;
    ThreadPool pool;
};
