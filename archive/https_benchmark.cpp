#include <iostream>
#include <chrono>
#include <cstdlib>

#include "../crawler/network_util.h"

struct Target {
   const char *host;
   const char *path;
};

int main() {
   Target targets[] = {
      {"www.example.com",   ""},
      {"www.google.com",    ""},
      // {"www.gnu.org",       ""},
      {"httpbin.org",       "html"},
      {"httpbin.org",       "get"},
      {"info.cern.ch",      ""},
   };
   int n = sizeof(targets) / sizeof(targets[0]);

   std::cout << "\n--- HTTPS GET Benchmark (sequential) ---\n\n";

   auto total_start = std::chrono::high_resolution_clock::now();

   for (int i = 0; i < n; ++i) {
      std::cout << "[" << (i + 1) << "/" << n << "] GET https://"
                << targets[i].host << "/" << targets[i].path << "\n";

      auto start = std::chrono::high_resolution_clock::now();

      size_t len = 0;
      char *body = https_get(targets[i].host, targets[i].path, &len);

      auto end = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double, std::milli> elapsed = end - start;

      if (body) {
         std::cout << "  Status: OK | Body size: " << len << " bytes | Time: "
                   << elapsed.count() << " ms\n";
         size_t preview = len < 200 ? len : 200;
         std::cout << "  Preview: ";
         std::cout.write(body, preview);
         std::cout << "\n  ...\n\n";
         free(body);
      } else {
         std::cout << "  Status: FAILED | Time: " << elapsed.count() << " ms\n\n";
      }
   }

   auto total_end = std::chrono::high_resolution_clock::now();
   std::chrono::duration<double, std::milli> total = total_end - total_start;

   std::cout << "--- Total sequential time: " << total.count() << " ms ---\n\n";

   return 0;
}
