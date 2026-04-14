#include <unistd.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lib/rpc_listener.h"
#include "lib/consts.h"
#include "lib/string.h"

static const char HOMEPAGE_HTML[] =
    "<!doctype html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "<meta charset=\"utf-8\">\n"
    "<title>Seamus the Search Engine</title>\n"
    "<style>\n"
    "  body {\n"
    "    margin: 0;\n"
    "    padding: 0;\n"
    "    min-height: 100vh;\n"
    "    display: flex;\n"
    "    flex-direction: column;\n"
    "    align-items: center;\n"
    "    background: linear-gradient(to top, #004d00 0%, #f4fdf4 100%);\n"
    "    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;\n"
    "  }\n"
    "  .header {\n"
    "    margin-top: 15vh;\n"
    "    text-align: center;\n"
    "  }\n"
    "  h1 {\n"
    "    font-size: 4rem;\n"
    "    color: #1a1a1a;\n"
    "    margin: 0;\n"
    "  }\n"
    "  .search-container {\n"
    "    flex: 1;\n"
    "    display: flex;\n"
    "    align-items: center;\n"
    "    justify-content: center;\n"
    "    width: 100%;\n"
    "  }\n"
    "  .search-box {\n"
    "    display: flex;\n"
    "    align-items: center;\n"
    "    width: 60%;\n"
    "    max-width: 800px;\n"
    "    box-shadow: 0 8px 24px rgba(0,0,0,0.15);\n"
    "    border-radius: 40px;\n"
    "    background: white;\n"
    "    overflow: hidden;\n"
    "  }\n"
    "  #q {\n"
    "    flex: 1;\n"
    "    border: none;\n"
    "    padding: 20px 30px;\n"
    "    font-size: 1.5rem;\n"
    "    outline: none;\n"
    "    background: transparent;\n"
    "  }\n"
    "  #go {\n"
    "    border: none;\n"
    "    background: transparent;\n"
    "    padding: 15px 25px;\n"
    "    cursor: pointer;\n"
    "    display: flex;\n"
    "    align-items: center;\n"
    "    justify-content: center;\n"
    "  }\n"
    "  #go img {\n"
    "    width: 30px;\n"
    "    height: 30px;\n"
    "    opacity: 0.7;\n"
    "    transition: opacity 0.2s;\n"
    "  }\n"
    "  #go:hover img {\n"
    "    opacity: 1;\n"
    "  }\n"
    "  .footer {\n"
    "    width: 100%;\n"
    "    display: flex;\n"
    "    justify-content: space-evenly;\n"
    "    padding: 30px 10px;\n"
    "    font-size: 0.9rem;\n"
    "    color: rgba(255, 255, 255, 0.9);\n"
    "    box-sizing: border-box;\n"
    "  }\n"
    "  .footer span {\n"
    "    white-space: nowrap;\n"
    "  }\n"
    "</style>\n"
    "</head>\n"
    "<body>\n"
    "  <div class=\"header\">\n"
    "    <h1>Seamus the Search Engine</h1>\n"
    "  </div>\n"
    "  <div class=\"search-container\">\n"
    "    <div class=\"search-box\">\n"
    "      <input id=\"q\" type=\"text\" autofocus>\n"
    "      <button id=\"go\"><img src=\"htmlserver/images/magnifying-glass.png\" alt=\"Search\"></button>\n"
    "    </div>\n"
    "  </div>\n"
    "  <div class=\"footer\">\n"
    "    <span>Erik Nielsen</span>\n"
    "    <span>Aiden Mizhen</span>\n"
    "    <span>David McDermott</span>\n"
    "    <span>Charles Huang</span>\n"
    "    <span>Hrishkesh Bagalkote</span>\n"
    "    <span>Esben Sorensen</span>\n"
    "  </div>\n"
    "  <script>\n"
    "    function submit(){\n"
    "      var v=document.getElementById('q').value.trim();\n"
    "      if(!v)return;\n"
    "      var term=encodeURIComponent(v);\n"
    "      window.location.href='/'+term;\n"
    "    }\n"
    "    document.getElementById('go').addEventListener('click',submit);\n"
    "    document.getElementById('q').addEventListener('keydown',function(e){\n"
    "      if(e.key==='Enter'){submit();}\n"
    "    });\n"
    "  </script>\n"
    "</body>\n"
    "</html>";

// Read the full HTTP request headers from the socket into a raw buffer,
static string read_request(int fd) {
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
        if (n <= 0) {
            free(buf);
            return string("");
        }
        
        for (ssize_t i = 0; i < n; ++i) {
            char c = buf[total + i];
            if (matched == 0 && c == '\r') matched = 1;
            else if (matched == 1 && c == '\n') matched = 2;
            else if (matched == 2 && c == '\r') matched = 3;
            else if (matched == 3 && c == '\n') matched = 4;
            else if (c == '\r') matched = 1;
            else matched = 0;
            
            if (matched == 4) {
                total += static_cast<size_t>(i + 1);
                break;
            }
        }
        if (matched != 4) total += static_cast<size_t>(n);
    }
    
    string req(buf, total);
    free(buf);
    return req;
}

static string_view parse_path(const string& request) {
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

static string percent_decode(const string_view& s) {
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
            int hi = hex(s[i + 1]);
            int lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                tmp[out_len++] = static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        tmp[out_len++] = s[i];
    }
    
    string out(tmp, out_len);
    free(tmp);
    return out;
}

static string parse_query_term(const string_view& path) {
    if (path.size() < 2 || path[0] != '/') return string("");
    
    size_t q_pos = 1;
    while (q_pos < path.size() && path[q_pos] != '?') q_pos++;
    
    return percent_decode(path.substr(1, q_pos - 1));
}

// strip out anything that isn't a letter, number, or space
static string remove_special_chars(const string& s) {
    size_t len = s.size();
    char* tmp = static_cast<char*>(malloc(len));
    size_t out_len = 0;
    
    for (size_t i = 0; i < len; ++i) {
        char c = s[i];
        if (isalnum((unsigned char)c) || c == ' ' || c == '/' || c == '-' || c == '-' || c =='.') {
            tmp[out_len++] = c;
        }
    }
    
    string out(tmp, out_len);
    free(tmp);
    return out;
}

static string escape_html(const string& s) {
    size_t max_len = s.size() * 5; 
    char* tmp = static_cast<char*>(malloc(max_len));
    size_t out_len = 0;
    
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        switch (c) {
            case '<': memcpy(tmp + out_len, "&lt;", 4); out_len += 4; break;
            case '>': memcpy(tmp + out_len, "&gt;", 4); out_len += 4; break;
            case '&': memcpy(tmp + out_len, "&amp;", 5); out_len += 5; break;
            default:  tmp[out_len++] = c;
        }
    }
    
    string out(tmp, out_len);
    free(tmp);
    return out;
}

static void send_all(int fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, data + sent, len - sent);
        if (n <= 0) return;
        sent += static_cast<size_t>(n);
    }
}

static void send_response(int fd, const char* status, string_view body) {
    char header[256];
    int hlen = snprintf(
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

static void send_image(int fd, const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        // This will tell us exactly why it's failing!
        printf("[htmlserver] ERROR: Could not open file on hard drive at '%s'\n", filepath);
        fflush(stdout);
        
        send_response(fd, "404 Not Found", string_view("Image not found", 15));
        return;
    }

    printf("[htmlserver] SUCCESS: Found and sending '%s'\n", filepath);
    fflush(stdout);

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* img_data = static_cast<char*>(malloc(fsize));
    fread(img_data, 1, fsize, f);
    fclose(f);

    char header[256];
    int hlen = snprintf(
        header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/png\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n",
        fsize);
        
    if (hlen > 0) {
        send_all(fd, header, static_cast<size_t>(hlen));
        send_all(fd, img_data, static_cast<size_t>(fsize));
    }
    
    free(img_data);
}

static void handle_client(int fd) {
    string request = read_request(fd);
    if (request.size() == 0) {
        close(fd);
        return;
    }

    string_view path = parse_path(request);

    if (path == "/") {
        // Serve the homepage
        send_response(fd, "200 OK", string_view(HOMEPAGE_HTML, sizeof(HOMEPAGE_HTML) - 1));
        
    } else if (path == "/favicon.ico") {
        // Ignore background icon requests
        send_response(fd, "404 Not Found", string_view("no favicon", 10));
        
    } else if (path.size() > 0 && path[0] == '/') {
        // 1. Clean the path FIRST (strips '?' query strings and percent-decodes)
        string raw_term = parse_query_term(path);
        
        // 2. NOW check if the cleaned term is asking for the image
        if (raw_term == "htmlserver/images/magnifying-glass.png") {
            send_image(fd, "htmlserver/images/magnifying-glass.png");
        } else {
            // 3. Otherwise, treat it as a search query!
            string term = remove_special_chars(raw_term);

            printf("[htmlserver] query term: [%.*s]\n", static_cast<int>(term.size()), term.data());
            fflush(stdout);

            string escaped_term = escape_html(term);
            
            string body = string::join("", 
                "<!doctype html><html><body><h3>Query term</h3><p>",
                escaped_term,
                "</p><a href=\"/\">back</a></body></html>"
            );
            
            send_response(fd, "200 OK", string_view(body.data(), body.size()));
        }
    } else {
        send_response(fd, "400 Bad Request", string_view("bad request", 11));
    }

    close(fd);
}

int main() {
    RPCListener listener(HTMLSERVER_PORT, HTMLSERVER_THREADS);
    if (!listener.valid()) {
        fprintf(stderr, "failed to bind port %u\n", HTMLSERVER_PORT);
        return 1;
    }
    printf("serving on http://0.0.0.0:%u\n", HTMLSERVER_PORT);
    fflush(stdout);

    listener.listener_loop(handle_client);
    return 0;
}