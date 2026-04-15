#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <cstddef>
#include "string.h"
#include <cstring>
#include <optional>

// Spin up a socket and connect. Returns the connected socket fd, or -1 on failure.
inline int connect_to_host(const string& host, uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.data(), &addr.sin_addr) <= 0) {
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Send a buffer to an end host over TCP
// This operation spins up a short-lived socket but is completely stateless beyond the success of the creation of the socket
inline bool send_buffer(const string& host, uint16_t port, const void* buf, size_t len) {
    int sockfd = connect_to_host(host, port);
    if (sockfd < 0) return false;

    bool result = send_exact(sockfd, buf, len);
    close(sockfd);
    return result;
}

inline bool send_exact(int fd, const void* buf, size_t len) {
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(fd, (const char*)buf + total_sent, len - total_sent, 0);
        if (sent <= 0) return false;
        total_sent += sent;
    }
    return true;
}

inline bool send_u32(int fd, uint32_t val) {
    uint32_t net = htonl(val);
    return send_exact(fd, &net, sizeof(uint32_t));
}

inline bool send_bool(int fd, bool value) {
    uint8_t network_val = value ? 1 : 0;
    return send_exact(fd, &network_val, sizeof(uint8_t));
}

inline bool send_string(int fd, const string& s) {
    if (!send_u32(fd, s.size())) return false; // Send 4-byte header
    return send_exact(fd, s.data(), s.size()); // Send characters
}

inline bool send_stringview(int fd, const string_view& sv) {
    if (!send_u32(fd, sv.size())) return false; // Send 4-byte header
    return send_exact(fd, sv.data(), sv.size()); // Send characters
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


// Read a network byte order uint16_t from fd into out
inline bool recv_u16(int fd, uint16_t& out) {
    uint16_t net;
    if (!recv_exact(fd, &net, sizeof(uint16_t))) return false;
    out = ntohs(net);
    return true;
}

inline std::optional<bool> recv_bool(int fd) {
    uint8_t value;
    if (!recv_exact(fd, &value, sizeof(uint8_t))) return std::nullopt;
    return value != 0;
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
