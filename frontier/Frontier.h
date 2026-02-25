#include <cstddef>    // for size_t
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <stdio.h>
#include <cstring>
#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/unordered_map.h"
#include "../lib/priority_queue.h"
#include "../lib/utils.h"

unordered_map<string,double> makeTldWeight();

bool is_digit(char c);

double max(double i, double j);

double calcPriorityScore(const string& url, int seed_list_dist);

struct UncrawledItem {
    string url;
    uint16_t seed_list_dist;
    uint16_t priority_score; // only to be set once inserted

    UncrawledItem(string init_url, uint16_t init_seed_list_dist) : url(static_cast<string&&>(init_url)), seed_list_dist(init_seed_list_dist),
        priority_score(calcPriorityScore(init_url, init_seed_list_dist)) { }
};

struct CrawledItem {
    string url;
    uint16_t seed_list_dist;
    uint32_t times_seen;

    CrawledItem(string init_url, uint16_t init_seed_list_dist, uint16_t times_seen_init) : url(static_cast<string&&>(init_url)), seed_list_dist(init_seed_list_dist),
        times_seen(times_seen_init) { }
};

struct UncrawledComp {
    bool operator()(const UncrawledItem& u1, const UncrawledItem& u2) const;
};

class Frontier {
private:
    priority_queue<UncrawledItem, vector<UncrawledItem>, UncrawledComp> pq;
    unordered_map<string, uint32_t> curr_urls;
    uint16_t worker_id;
public:
    Frontier(uint16_t worker_id_init, size_t initial_map_size = 2048, double initial_loading_factor = 0.65) 
        : curr_urls(initial_map_size, initial_loading_factor), worker_id(worker_id_init) { }

    void push(const UncrawledItem &u);

    void push(string &url, int seed_list_dist);

    void pop();

    CrawledItem front();

    void persist();

    size_t size();
};