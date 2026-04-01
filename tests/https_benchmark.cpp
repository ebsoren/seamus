#include <cstdio>
#include <cstring>
#include "../crawler/network_util.h"
#include "../lib/consts.h"
#include "../lib/utils.h"
#include "../lib/string.h"


struct TestCase {
    const char *host;
    const char *path;
};

int main() {
    printf("===== DIRECT CALLS (C string literals) =====\n\n");

    TestCase cases[] = {
        {"en.wikipedia.org",  ""},
        {"en.wikipedia.org",  "wiki/Main_Page"},
        {"www.google.com",    ""},
        {"example.com",       ""},
        {"httpbin.org",       "get"},
    };

    char body[MAX_HTML_SIZE];

    for (const auto &tc : cases) {
        printf("========================================\n");
        printf("GET https://%s/%s\n", tc.host, tc.path);
        printf("========================================\n");

        memset(body, 0, sizeof(body));
        ssize_t len = https_get(tc.host, tc.path, body);

        if (len < 0) {
            printf("FAILED (returned %zd)\n\n", len);
        } else if (len == 0) {
            printf("EMPTY BODY (0 bytes)\n\n");
        } else {
            size_t preview = len < 500 ? static_cast<size_t>(len) : 500;
            printf("OK — %zd bytes. First %zu bytes:\n", len, preview);
            fwrite(body, 1, preview, stdout);
            printf("\n\n");
        }
    }
}
