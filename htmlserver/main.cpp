#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <chrono>
#include <memory>

#include "../crawler/network_util.h"
#include "../lib/rpc_listener.h"
#include "../lib/consts.h"
#include "../lib/string.h"
#include "../lib/vector.h"     
#include "../lib/algorithm.h"   
#include "../ranker/Ranker.h"
#include "../query/query_handler.h"  
#include "html_templates.h"

// HTML Builder
struct HtmlBuilder {
    char* data;
    size_t len;
    size_t cap;
    
    HtmlBuilder() : len(0), cap(4096) { 
        data = static_cast<char*>(malloc(cap)); 
    }
    
    ~HtmlBuilder() { free(data); }
    
    void append(const char* str, size_t n) {
        if (len + n > cap) {
            while (len + n > cap) cap *= 2;
            data = static_cast<char*>(realloc(data, cap));
        }
        memcpy(data + len, str, n);
        len += n;
    }
    
    void append(const char* str) { append(str, strlen(str)); }
    void append(const string& s) { append(s.data(), s.size()); }
    void append(const string_view& sv) { append(sv.data(), sv.size()); }
    
    void append(int n) {
        char buf[32];
        int l = snprintf(buf, sizeof(buf), "%d", n);
        if (l > 0) append(buf, static_cast<size_t>(l));
    }
};

// Main Server
class SearchServer {
public:
    SearchServer() {
        // Initialize components here
        url_store = new UrlStore(nullptr, 0);
        index_manager = new IndexManager(url_store);
        index_server = new IndexServer(index_manager);
        query_handler = new QueryHandler(index_server);
    }

    void run(uint16_t port, int threads) {
        RPCListener listener(port, threads);
        if (!listener.valid()) {
            fprintf(stderr, "failed to bind port %u\n", port);
            exit(1);
        }
        printf("Seamus Search Engine serving on http://0.0.0.0:%u\n", port);
        fflush(stdout);

        listener.listener_loop([this](int fd) {
            this->handle_client(fd);
        });
    }

private:
    QueryHandler * query_handler;
    UrlStore * url_store;
    IndexManager * index_manager;
    IndexServer * index_server;

    void handle_client(int fd) {
        string request = read_request(fd);
        if (request.size() == 0) {
            close(fd);
            return;
        }

        string_view path = parse_path(request);

        if (path == "/") {
            send_response(fd, "200 OK", string_view(HOMEPAGE_HTML, strlen(HOMEPAGE_HTML)));
        } 
        else if (is_static_asset(path)) {
            string local_path = (path[0] == '/') ? string(path.substr(1, path.size()-1).data(), path.size()-1) : string(path.data(), path.size());
            send_image(fd, local_path.data());
        } 
        else if (path.size() > 0 && path[0] == '/') {
            serve_search_results(fd, path);
        } 
        else {
            send_response(fd, "400 Bad Request", string_view("bad request", 11));
        }
        close(fd);
    }

    string get_accurate_title(const string &url) {
        if (url.size() < 9) return string("");

        string host = extract_host(url);
        const char* slash = static_cast<const char*>(memchr(url.data() + 8, '/', url.size() - 8));
        const char* path_str = slash ? slash + 1 : "";

        auto buf = std::make_unique<char[]>(MAX_HTML_SIZE);
        ssize_t html_len = https_get(host.data(), path_str, buf.get());

        if (html_len <= 0) return string("");

        const char* p = buf.get();
        const char* end = buf.get() + html_len;

        while (p + 7 <= end) {
            if (!strncasecmp(p, "<title>", 7)) {
                p += 7;
                const char* title_start = p;
                while (p + 8 <= end && strncasecmp(p, "</title>", 8)) p++;
                if (p + 8 <= end) {
                    return string(title_start, p - title_start);
                }
                return string("");
            }
            p++;
        }
        return string("");
    }

    void serve_search_results(int fd, string_view path) {
        string raw_term = parse_query_term(path);
        string term = remove_special_chars(raw_term);
        int current_page = get_page_number(path);

        size_t q_pos = 0;
        while (q_pos < path.size() && path[q_pos] != '?') q_pos++;
        string_view base_url = path.substr(0, q_pos);

        fprintf(stderr, "[HTMLSERVER] query term='%.*s' (len=%zu)\n",
                static_cast<int>(term.size()), term.data(), term.size());
        fflush(stderr);

        auto start_time = std::chrono::high_resolution_clock::now();

        vector<LeanPage> results = query_handler->handle_client_req(term);

        auto end_time = std::chrono::high_resolution_clock::now();

        fprintf(stderr, "[HTMLSERVER] got %zu results\n", results.size());
        for (size_t i = 0; i < results.size() && i < 3; ++i) {
            fprintf(stderr, "[HTMLSERVER]   result[%zu] url='%.*s' score=%.4f\n",
                    i, static_cast<int>(results[i].url.size()), results[i].url.data(), results[i].score);
        }
        fflush(stderr);
        int duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        HtmlBuilder html;
        html.append(RESULTS_PART_1);
        
        html.append("<input type=\"text\" class=\"search-box\" value=\"");
        html.append(html_escape(term));
        html.append("\" readonly>\n<a href=\"/\" class=\"back-btn\">BACK</a>\n");
        html.append("<h1 class=\"header-title\">Seamus the Search Engine</h1>\n</div>\n");

        html.append("<div class=\"results-container\">\n");
        size_t start_idx = (current_page - 1) * 10;
        
        // Use your custom min algorithm from lib/algorithm.h
        size_t end_idx = min(start_idx + 10, results.size());

        if (start_idx < results.size()) {
            for (size_t i = start_idx; i < end_idx; ++i) {
                const auto& res = results[i];

                string real_title = get_accurate_title(res.url);
                const string& display_title = (real_title.size() > 0) ? real_title : res.title;

                html.append("<div class=\"result-item\">\n");
                html.append("<a class=\"result-title\" href=\""); html.append(res.url); html.append("\" target=\"_blank\">");
                html.append(display_title); html.append("</a>\n");
                html.append("<a class=\"result-url\" href=\""); html.append(res.url); html.append("\" target=\"_blank\">");
                html.append(res.url); html.append("</a>\n</div>\n");
            }
        }
        html.append("</div>\n");

        html.append("<div class=\"pagination\">\n");
        if (current_page > 1) {
            html.append("<a class=\"page-btn\" href=\""); html.append(base_url);
            html.append("?p="); html.append(current_page - 1); html.append("\">&laquo; Previous</a>\n");
        }
        if (end_idx < results.size()) {
            html.append("<a class=\"page-btn\" href=\""); html.append(base_url);
            html.append("?p="); html.append(current_page + 1); html.append("\">Next &raquo;</a>\n");
        }
        html.append("</div>\n");

        html.append(RESULTS_PART_2);
        html.append(duration_ms);
        html.append(RESULTS_PART_3);

        send_response(fd, "200 OK", string_view(html.data, html.len));
    }

    bool is_static_asset(string_view path) {
        return (path.size() >= 4 && path.substr(path.size() - 4, 4) == ".png") || path == "/favicon.ico";
    }

    string read_request(int fd) {
        size_t capacity = 4096;
        char* buf = static_cast<char*>(malloc(capacity));
        size_t total = 0;
        int matched = 0; 
        while (matched < 4) {
            if (total == capacity) {
                capacity *= 2;
                if (capacity > (1 << 16)) { free(buf); return string(""); }
                buf = static_cast<char*>(realloc(buf, capacity));
            }
            ssize_t n = read(fd, buf + total, capacity - total);
            if (n <= 0) { free(buf); return string(""); }
            for (ssize_t i = 0; i < n; ++i) {
                char c = buf[total + i];
                if (matched == 0 && c == '\r') matched = 1;
                else if (matched == 1 && c == '\n') matched = 2;
                else if (matched == 2 && c == '\r') matched = 3;
                else if (matched == 3 && c == '\n') matched = 4;
                else if (c == '\r') matched = 1;
                else matched = 0;
                if (matched == 4) { total += static_cast<size_t>(i + 1); break; }
            }
            if (matched != 4) total += static_cast<size_t>(n);
        }
        string req(buf, total);
        free(buf);
        return req;
    }

    string_view parse_path(const string& request) {
        const char* data = request.data();
        size_t len = request.size();
        size_t sp1 = 0;
        while (sp1 < len && data[sp1] != ' ') sp1++;
        if (sp1 == len) return string_view();
        size_t sp2 = sp1 + 1;
        while (sp2 < len && data[sp2] != ' ') sp2++;
        if (sp2 == len) return string_view();
        return request.str_view(sp1 + 1, sp2 - sp1 - 1);
    }

    string percent_decode(const string_view& s) {
        size_t len = s.size();
        char* tmp = static_cast<char*>(malloc(len));
        size_t out_len = 0;
        for (size_t i = 0; i < len; ++i) {
            if (s[i] == '%' && i + 2 < len) {
                auto hex = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
                    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
                    return -1;
                };
                int hi = hex(s[i + 1]), lo = hex(s[i + 2]);
                if (hi >= 0 && lo >= 0) {
                    tmp[out_len++] = static_cast<char>((hi << 4) | lo);
                    i += 2; continue;
                }
            }
            tmp[out_len++] = (s[i] == '+') ? ' ' : s[i];
        }
        string out(tmp, out_len);
        free(tmp);
        return out;
    }

    string parse_query_term(const string_view& path) {
        if (path.size() < 2 || path[0] != '/') return string("");
        size_t q_pos = 1;
        while (q_pos < path.size() && path[q_pos] != '?') q_pos++;
        return percent_decode(path.substr(1, q_pos - 1));
    }

    string remove_special_chars(const string& s) {
        size_t len = s.size();
        char* tmp = static_cast<char*>(malloc(len));
        size_t out_len = 0;

        for (size_t i = 0; i < len; ++i) {
            char c = s[i];
            if (c == '\0') continue;

            // Convert uppercase letters to lowercase
            if (c >= 'A' && c <= 'Z') {
                c += ('a' - 'A');
            }

            // Strictly keep only a-z, 0-9, spaces, and query syntax characters
            if ((c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == ' ' ||
                c == '"' ||
                c == '|' ||
                c == '-' ||
                c == '(' ||
                c == ')') {
                tmp[out_len++] = c;
            }
        }

        string out(tmp, out_len);
        free(tmp);
        return out;
    }

    string html_escape(const string& s) {
        size_t len = s.size();
        char* tmp = static_cast<char*>(malloc(len * 6));
        size_t out_len = 0;
        for (size_t i = 0; i < len; i++) {
            char c = s.data()[i];
            if (c == '"') {
                memcpy(tmp + out_len, "&quot;", 6); out_len += 6;
            } else if (c == '&') {
                memcpy(tmp + out_len, "&amp;", 5); out_len += 5;
            } else if (c == '<') {
                memcpy(tmp + out_len, "&lt;", 4); out_len += 4;
            } else if (c == '>') {
                memcpy(tmp + out_len, "&gt;", 4); out_len += 4;
            } else {
                tmp[out_len++] = c;
            }
        }
        string out(tmp, out_len);
        free(tmp);
        return out;
    }

    void send_response(int fd, const char* status, string_view body) {
        char header[256];
        int hlen = snprintf(header, sizeof(header),
            "HTTP/1.1 %s\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
            status, body.size());
        write(fd, header, hlen);
        write(fd, body.data(), body.size());
    }

    void send_image(int fd, const char* filepath) {
        FILE* f = fopen(filepath, "rb");
        if (!f) {
            send_response(fd, "404 Not Found", string_view("Image not found", 15));
            return;
        }
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* img_data = static_cast<char*>(malloc(fsize));
        fread(img_data, 1, fsize, f);
        fclose(f);

        char header[256];
        int hlen = snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: %ld\r\nConnection: close\r\n\r\n",
            fsize);
        write(fd, header, hlen);
        write(fd, img_data, fsize);
        free(img_data);
    }

    int get_page_number(const string_view& path) {
        int page = 1;
        const char* data = path.data();
        size_t len = path.size();
        for (size_t i = 0; i < len; ++i) {
            if (data[i] == 'p' && i + 1 < len && data[i+1] == '=') {
                if (i == 0 || data[i-1] == '?' || data[i-1] == '&') {
                    page = atoi(data + i + 2);
                    if (page < 1) page = 1;
                    break;
                }
            }
        }
        return page;
    }

    vector<LeanPage> resultsTest() {
        vector<LeanPage> res;
        for(int i=0; i<15; ++i) res.push_back(LeanPage{string("https://roar.com"), string("Roar R Us"), 0.5});
        return res;
    }
};

int main() {
    signal(SIGPIPE, SIG_IGN);
    SearchServer engine;
    engine.run(HTMLSERVER_PORT, HTMLSERVER_THREADS);
    return 0;
}