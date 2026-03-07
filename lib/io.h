#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <errno.h>

// Define standard read/write operations to and from file descriptors.

inline ssize_t seamus_read(int fd, char *buf, size_t len) {
    ssize_t bytes_read = 0;
    ssize_t bytes_in = 0;
    while ((bytes_in = recv(fd, buf + bytes_read, len - bytes_read, 0))) {
        if (bytes_in == -1 && errno == EINTR) continue;
        if (bytes_in <= 0) break;
        bytes_read += bytes_in;
    }

    if (bytes_in < 0) return -1;
    if (bytes_read == 0) return 0;
    return bytes_read;
}

inline ssize_t seamus_write(int fd, const char *buf, size_t len) {
    ssize_t written = 0;
    while (written < len) {
        ssize_t w = write(fd, buf + written, len - written);
        if (w == -1) {
            if (errno == EINTR) continue;
            throw std::runtime_error("seamus_write: write failed");
        }
        written += w;
    }
    return written;
}