#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <cstddef>
#include "string.h"


// Send a buffer to an end host over TCP
// This operation spins up a short-lived socket but is completely stateless beyond the success of the creation of the socket
bool send_buffer(const string& host, uint16_t port, const void* buf, size_t len) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.cstr(), &addr.sin_addr) <= 0) {
        close(sockfd);
        return false;
    }

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return false;
    }

    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(sockfd, (const char*)buf + total_sent, len - total_sent, 0);
        if (sent <= 0) {
            close(sockfd);
            return false;
        }
        total_sent += sent;
    }

    close(sockfd);
    return true;
}
