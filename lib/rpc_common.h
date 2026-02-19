#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <cstddef>
#include "string.h"
#include <cstring>
#include <optional>


// Send a buffer to an end host over TCP
// This operation spins up a short-lived socket but is completely stateless beyond the success of the creation of the socket
inline bool send_buffer(const string& host, uint16_t port, const void* buf, size_t len) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return false;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.data(), &addr.sin_addr) <= 0) {
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


// Receive exactly `len` bytes from an ephemeral socket fd into buf
// Useful for wire formats where a fixed size metadata region tells us the variable size of a subsequent data region
inline bool recv_exact(int fd, void* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t received = recv(fd, static_cast<char*>(buf) + total, len - total, 0);
        if (received <= 0) return false;
        total += static_cast<size_t>(received);
    }
    return true;
}


// Read a network byte order uint32_t from fd into out
inline bool recv_u32(int fd, uint32_t& out) {
    uint32_t net;
    if (!recv_exact(fd, &net, sizeof(uint32_t))) return false;
    out = ntohl(net);
    return true;
}


// Read a length-prefixed string from fd
inline std::optional<string> recv_string(int fd) {
    uint32_t len;
    if (!recv_u32(fd, len)) return std::nullopt;
    char* buf = new char[len + 1];
    buf[len] = '\0';
    if (!recv_exact(fd, buf, len)) {
        delete[] buf;
        return std::nullopt;
    }
    string s(buf, len);
    delete[] buf;
    return std::optional<string>(std::move(s));
}
