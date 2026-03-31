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


// Returns a malloc'd buffer containing the HTTP response body (caller must free),
// or nullptr on failure. Sets *out_len to the length of the body if non-null.
// inline char *http_get(const char *host, const char *path, size_t *out_len) {
//    struct addrinfo hints, *res;
//    memset(&hints, 0, sizeof(hints));
//    hints.ai_family = AF_INET;
//    hints.ai_socktype = SOCK_STREAM;
//    hints.ai_protocol = IPPROTO_TCP;
//
//    if (getaddrinfo(host, "80", &hints, &res) != 0)
//       return nullptr;
//
//    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
//    if (sock < 0) {
//       freeaddrinfo(res);
//       return nullptr;
//    }
//
//    // Set a 5-second timeout on send and recv
//    struct timeval tv;
//    tv.tv_sec = 5;
//    tv.tv_usec = 0;
//    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
//    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
//
//    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
//       close(sock);
//       freeaddrinfo(res);
//       return nullptr;
//    }
//    freeaddrinfo(res);
//
//    // Build GET request with snprintf
//    char req[1024];
//    int req_len = snprintf(req, sizeof(req),
//       "GET /%s HTTP/1.1\r\n"
//       "Host: %s\r\n"
//       "User-Agent: Seamus the Search Engine (web crawler for university course)\r\n"
//       "Accept: */*\r\n"
//       "Accept-Encoding: identity\r\n"
//       "Connection: close\r\n\r\n",
//       path, host);
//
//    size_t total_sent = 0;
//    while (total_sent < (size_t)req_len) {
//       int sent = send(sock, req + total_sent, req_len - total_sent, 0);
//       if (sent <= 0) {
//          close(sock);
//          return nullptr;
//       }
//       total_sent += sent;
//    }
//
//    // Read full response into a growable buffer
//    size_t resp_cap = 16384;
//    size_t resp_len = 0;
//    char *response = (char *)malloc(resp_cap);
//    if (!response) {
//       close(sock);
//       return nullptr;
//    }
//
//    int rcvd;
//    while ((rcvd = recv(sock, response + resp_len, resp_cap - resp_len, 0)) > 0) {
//       resp_len += rcvd;
//       if (resp_len == resp_cap) {
//          resp_cap *= 2;
//          char *grown = (char *)realloc(response, resp_cap);
//          if (!grown) {
//             free(response);
//             close(sock);
//             return nullptr;
//          }
//          response = grown;
//       }
//    }
//
//    close(sock);
//
//    // Strip HTTP headers — find "\r\n\r\n"
//    char *header_end = nullptr;
//    for (size_t i = 0; i + 3 < resp_len; ++i) {
//       if (response[i] == '\r' && response[i+1] == '\n' &&
//           response[i+2] == '\r' && response[i+3] == '\n') {
//          header_end = response + i;
//          break;
//       }
//    }
//
//    size_t body_start = header_end ? (header_end - response + 4) : 0;
//    size_t body_len = resp_len - body_start;
//
//    char *result = (char *)malloc(body_len + 1);
//    if (!result) {
//       free(response);
//       return nullptr;
//    }
//
//    memcpy(result, response + body_start, body_len);
//    result[body_len] = '\0';
//    free(response);
//
//    if (out_len)
//       *out_len = body_len;
//
//    return result;
// }


// Writes the HTTPS response body into caller-provided buf (must be MAX_HTML_SIZE bytes).
// Returns the number of bytes written, or -1 on failure.
inline ssize_t https_get(const char *host, const char *path, char *body) {
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

   // Set up SSL
   SSL_library_init();
   SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
   if (!ctx) {
      close(sock);
      return -1;
   }

   SSL *ssl = SSL_new(ctx);
   SSL_set_tlsext_host_name(ssl, host);
   SSL_set_fd(ssl, sock);

   if (SSL_connect(ssl) <= 0) {
      SSL_free(ssl);
      SSL_CTX_free(ctx);
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
         SSL_CTX_free(ctx);
         close(sock);
         return -1;
      }
      total_sent += sent;
   }

   // Read response directly into caller's buffer
   size_t resp_len = 0;

   int rcvd;
   while ((rcvd = SSL_read(ssl, body + resp_len, MAX_HTML_SIZE - resp_len)) > 0) {
      resp_len += rcvd;
      if (resp_len >= MAX_HTML_SIZE) {
         break;
      }
   }

   SSL_shutdown(ssl);
   SSL_free(ssl);
   SSL_CTX_free(ctx);
   close(sock);

   return static_cast<ssize_t>(resp_len);
}
