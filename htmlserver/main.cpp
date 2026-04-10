#include <unistd.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "lib/rpc_listener.h"
#include "lib/consts.h"

static constexpr char TERM_SEPARATOR = '+';

static const char* HOMEPAGE_HTML =
    "<!doctype html>"
    "<html><head><title>seamus</title></head><body>"
    "<input id=\"q\" type=\"text\" autofocus>"
    "<button id=\"go\">Search</button>"
    "<script>"
    "function submit(){"
    "  var v=document.getElementById('q').value.trim();"
    "  if(!v)return;"
    "  var terms=v.split(/\\s+/).map(encodeURIComponent).join('+');"
    "  window.location.href='/'+terms;"
    "}"
    "document.getElementById('go').addEventListener('click',submit);"
    "document.getElementById('q').addEventListener('keydown',function(e){"
    "  if(e.key==='Enter'){submit();}"
    "});"
    "</script>"
    "</body></html>";

// Read the full HTTP request headers from the socket (until CRLFCRLF).
static bool read_request(int fd, std::string& out) {
    char buf[4096];
    out.clear();
    while (out.find("\r\n\r\n") == std::string::npos) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) return false;
        out.append(buf, static_cast<size_t>(n));
        if (out.size() > (1 << 16)) return false;
    }
    return true;
}

// Extract the request target (path) from the HTTP request line.
static std::string parse_path(const std::string& request) {
    size_t sp1 = request.find(' ');
    if (sp1 == std::string::npos) return "";
    size_t sp2 = request.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) return "";
    return request.substr(sp1 + 1, sp2 - sp1 - 1);
}

// Percent-decode a single URL path component (also turns '+' into space only if
// you pass a query-string; we do NOT decode '+' here — it is our term separator).
static std::string percent_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + c - 'a';
                if (c >= 'A' && c <= 'F') return 10 + c - 'A';
                return -1;
            };
            int hi = hex(s[i + 1]);
            int lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

// Split the path (minus its leading '/') on TERM_SEPARATOR into decoded terms.
static std::vector<std::string> parse_query_terms(const std::string& path) {
    std::vector<std::string> terms;
    if (path.size() < 2 || path[0] != '/') return terms;
    std::string body = path.substr(1);
    // strip a trailing query string if present
    size_t q = body.find('?');
    if (q != std::string::npos) body.resize(q);

    size_t start = 0;
    while (start <= body.size()) {
        size_t end = body.find(TERM_SEPARATOR, start);
        if (end == std::string::npos) end = body.size();
        if (end > start) terms.push_back(percent_decode(body.substr(start, end - start)));
        start = end + 1;
    }
    return terms;
}

static void send_all(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, data + sent, len - sent);
        if (n <= 0) return;
        sent += static_cast<size_t>(n);
    }
}

static void send_response(int fd, const char* status, const std::string& body) {
    char header[256];
    int hlen = std::snprintf(
        header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, body.size());
    if (hlen <= 0) return;
    send_all(fd, header, static_cast<size_t>(hlen));
    send_all(fd, body.data(), body.size());
}

static void handle_client(int fd) {
    std::string request;
    if (!read_request(fd, request)) {
        close(fd);
        return;
    }

    std::string path = parse_path(request);

    if (path == "/") {
        send_response(fd, "200 OK", HOMEPAGE_HTML);
    } else if (!path.empty() && path[0] == '/') {
        std::vector<std::string> terms = parse_query_terms(path);

        std::printf("[htmlserver] query terms (%zu):", terms.size());
        for (const auto& t : terms) std::printf(" [%s]", t.c_str());
        std::printf("\n");
        std::fflush(stdout);

        std::string body = "<!doctype html><html><body><h3>Query terms</h3><ol>";
        for (const auto& t : terms) {
            body += "<li>";
            for (char c : t) {
                switch (c) {
                    case '<': body += "&lt;"; break;
                    case '>': body += "&gt;"; break;
                    case '&': body += "&amp;"; break;
                    default:  body += c;
                }
            }
            body += "</li>";
        }
        body += "</ol><a href=\"/\">back</a></body></html>";
        send_response(fd, "200 OK", body);
    } else {
        send_response(fd, "400 Bad Request", std::string("bad request"));
    }

    close(fd);
}

int main() {
    RPCListener listener(HTMLSERVER_PORT, HTMLSERVER_THREADS);
    if (!listener.valid()) {
        std::fprintf(stderr, "failed to bind port %u\n", HTMLSERVER_PORT);
        return 1;
    }
    std::printf("serving on http://0.0.0.0:%u\n", HTMLSERVER_PORT);
    std::fflush(stdout);

    listener.listener_loop(handle_client);
    return 0;
}
