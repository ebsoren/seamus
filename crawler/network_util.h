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


// Writes the HTTPS response body into caller-provided buf (must be MAX_HTML_SIZE bytes).
// Returns the number of bytes written, or -1 on failure.
inline ssize_t https_get(const char *host, const char *path, char *body) {
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
   close(sock);

   return static_cast<ssize_t>(resp_len);
}
