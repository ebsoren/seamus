#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include "../lib/consts.h"


// One-time SSL initialization and shared context (thread-safe via static local)
inline SSL_CTX* get_ssl_ctx() {
    static SSL_CTX* ctx = []() {
        SSL_library_init();
        SSL_CTX* c = SSL_CTX_new(SSLv23_method());
        return c;
    }();
    return ctx;
}


// Single HTTPS request. Reads headers + body into buf.
// Returns bytes of body written, or -1 on failure.
// If 'redirect_location' is non-null and the response is 3xx, extracts the Location header into it.
inline ssize_t https_get_once(const char *host, const char *path, char *body, char *redirect_location) {
    SSL_CTX *ctx = get_ssl_ctx();
    if (!ctx) return -1;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, "443", &hints, &res) != 0)
        return -1;

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(res);
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);

    SSL *ssl = SSL_new(ctx);
    SSL_set_tlsext_host_name(ssl, host);
    SSL_set_fd(ssl, sock);

    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl);
        close(sock);
        return -1;
    }

    // Build GET request
    char req[1024];
    int req_len = snprintf(req, sizeof(req),
        "GET /%s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Seamus the Search Engine (web crawler for university course)\r\n"
        "Accept: */*\r\n"
        "Accept-Encoding: identity\r\n"
        "Connection: close\r\n\r\n",
        path, host);

    int total_sent = 0;
    while (total_sent < req_len) {
        int sent = SSL_write(ssl, req + total_sent, req_len - total_sent);
        if (sent <= 0) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(sock);
            return -1;
        }
        total_sent += sent;
    }

    // Read response into caller's buffer, stripping HTTP headers
    size_t resp_len = 0;
    bool headers_stripped = false;
    bool is_redirect = false;

    int rcvd;
    while ((rcvd = SSL_read(ssl, body + resp_len, MAX_HTML_SIZE - resp_len)) > 0) {
        resp_len += rcvd;

        // Look for end of HTTP headers (\r\n\r\n) in what we've read so far
        if (!headers_stripped) {
            for (size_t i = 0; i + 3 < resp_len; ++i) {
                if (body[i] == '\r' && body[i+1] == '\n' && body[i+2] == '\r' && body[i+3] == '\n') {
                    // Check for 3xx redirect status in the first line (e.g. "HTTP/1.1 301 ...")
                    if (redirect_location && resp_len >= 12 && body[9] == '3') {
                        is_redirect = true;
                        // Extract Location header value
                        const char *loc = nullptr;
                        for (size_t j = 0; j + 10 < i; ++j) {
                            if ((body[j] == '\n') &&
                                (body[j+1] == 'L' || body[j+1] == 'l') &&
                                (body[j+2] == 'o' || body[j+2] == 'O') &&
                                (body[j+3] == 'c' || body[j+3] == 'C') &&
                                (body[j+4] == 'a' || body[j+4] == 'A') &&
                                (body[j+5] == 't' || body[j+5] == 'T') &&
                                (body[j+6] == 'i' || body[j+6] == 'I') &&
                                (body[j+7] == 'o' || body[j+7] == 'O') &&
                                (body[j+8] == 'n' || body[j+8] == 'N') &&
                                body[j+9] == ':') {
                                loc = body + j + 10;
                                while (*loc == ' ') loc++;
                                break;
                            }
                        }
                        if (loc) {
                            const char *end = static_cast<const char*>(memchr(loc, '\r', body + i - loc));
                            size_t len = end ? static_cast<size_t>(end - loc) : 0;
                            if (len > 0 && len < 1024) {
                                memcpy(redirect_location, loc, len);
                                redirect_location[len] = '\0';
                            }
                        }
                    }

                    size_t body_start = i + 4;
                    size_t body_len = resp_len - body_start;
                    memmove(body, body + body_start, body_len);
                    resp_len = body_len;
                    headers_stripped = true;
                    break;
                }
            }
        }

        if (is_redirect) break;  // Don't bother reading redirect body
        if (resp_len >= MAX_HTML_SIZE) break;
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);

    if (!headers_stripped) return -1;
    if (is_redirect) return -2;  // Signal redirect to caller
    return static_cast<ssize_t>(resp_len);
}

// Writes the HTTPS response body into caller-provided buf (must be MAX_HTML_SIZE bytes).
// Follows up to 5 redirects. Returns the number of bytes written, or -1 on failure.
inline ssize_t https_get(const char *host, const char *path, char *body) {
    char current_host[256];
    char current_path[4096];
    snprintf(current_host, sizeof(current_host), "%s", host);
    snprintf(current_path, sizeof(current_path), "%s", path);

    for (int redirects = 0; redirects < 5; ++redirects) {
        char location[1024] = {};
        ssize_t result = https_get_once(current_host, current_path, body, location);

        if (result != -2) return result;  // Not a redirect, return as-is
        if (location[0] == '\0') return -1;  // Redirect but no Location header

        // Parse the Location header
        if (strncmp(location, "https://", 8) == 0) {
            // Absolute URL — extract new host and path
            const char *host_start = location + 8;
            const char *slash = strchr(host_start, '/');
            if (slash) {
                size_t hlen = static_cast<size_t>(slash - host_start);
                if (hlen >= sizeof(current_host)) return -1;
                memcpy(current_host, host_start, hlen);
                current_host[hlen] = '\0';
                snprintf(current_path, sizeof(current_path), "%s", slash + 1);
            } else {
                snprintf(current_host, sizeof(current_host), "%s", host_start);
                current_path[0] = '\0';
            }
        } else if (location[0] == '/') {
            // Relative to host root
            snprintf(current_path, sizeof(current_path), "%s", location + 1);
        } else {
            // Relative path (uncommon)
            snprintf(current_path, sizeof(current_path), "%s", location);
        }
    }

    return -1;  // Too many redirects
}
