#include <cstddef>    // for size_t
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <stdio.h>
#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/unordered_map.h"
#include "../lib/priority_queue.h"
#include "../lib/utils.h"
#include "Frontier.h"


unordered_map<string,double> makeTldWeight() {
    unordered_map<string, double> m(32);

    m.insert("gov",1.2);
    m.insert("edu",1.2);
    m.insert("mil",1.2);
    m.insert("com",1.0);
    m.insert("org",1.0);
    m.insert("net",1.0);
    m.insert("info",0.8);
    m.insert("biz",0.8);
    m.insert("tk",0.8);

    return m;
}
static const auto tldWeight = makeTldWeight(); // factory function to avoid having to implement initializer lists lol

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

double max(double i, double j) {
    if(i > j) {
        return j;
    } else {
        return i;
    }
}

double calcPriorityScore(const string& url, int seed_list_dist) {
    // points for http or https however https > http
    double factor_1;

    if(url.substr(0,4) == "http") {
        if(url[4] == 's') {
            factor_1 = 1.0;
        } else {
            factor_1 = 0.6;
        }
    } else {
        return 0.0;
    }

    // points for certain desirable domains

    size_t start_pos = (factor_1 == 0.6) ? 7 : 8;
    size_t end_pos_domain = url.size();

    int subdomain_count = 0;
    int digit_count_domain = 0;
    double domain_size = 0.0;
    string extension = "";
    for(int i = start_pos; i < url.size(); i++) {
        if(url[i] == '/' || url[i] == '?' || url[i] == '#' || url[i] == ':') {
            end_pos_domain = i;
            break;
        } else if(url[i] == '.') {
            subdomain_count++;
            extension = "";
        } else {
            extension = extension + url[i];
            if(is_digit(url[i])) {
                digit_count_domain++;
            }
        }
        domain_size += 1.0;
    }
    auto slot = tldWeight.find(extension);
    double factor_2 = (slot == nullptr) ? 0.6 : slot->value; 

    // points for closer to seed list

    const double e = 2.718281828459;
    double factor_3 = double_pow(e, -0.05 * seed_list_dist);  // NOTE: may need to tune the constant here

    // points for shortness of domain title

    double factor_4 = max((50.0 - domain_size) / 50.0, 0.5);

    // points for less subdomains

    double factor_5 = (subdomain_count >= 5) ? 0.7 : 1;

    // digit count in domain name hurts the score

    double factor_6 = (digit_count_domain >= 3) ? 0.7 : 1;

    // points for shortness of overall url

    double factor_7 = max((150.0 - url.size()) / 100.0, 0.5);
    
    return int((factor_1 * factor_2 * factor_3 * factor_4 * factor_5 * factor_6 * factor_7) * 10000.0);
}


UncrawledItem::UncrawledItem(const string &init_url, uint16_t init_seed_list_dist) : url(init_url), seed_list_dist(init_seed_list_dist),
    priority_score(calcPriorityScore(init_url, init_seed_list_dist)) { }



CrawledItem::CrawledItem(const string &init_url, uint16_t init_seed_list_dist, uint16_t times_seen_init) : url(init_url), seed_list_dist(init_seed_list_dist),
    times_seen(times_seen_init) { }



bool UncrawledComp::operator()(const UncrawledItem& u1, const UncrawledItem& u2) const {
    return u1.priority_score < u2.priority_score;
}


Frontier::Frontier(uint16_t worker_id_init, size_t initial_map_size = 2048, double initial_loading_factor = 0.65) 
    : curr_urls(initial_map_size, initial_loading_factor), worker_id(worker_id_init) { }

void Frontier::push(const UncrawledItem &u) {
    uint32_t& count = curr_urls[u.url];
    
    if (count++ == 0) {
        pq.push(u);
    }
}

void Frontier::push(const string &url, int seed_list_dist) {
    uint32_t& count = curr_urls[url];
    
    if (count++ == 0) {
        pq.push(UncrawledItem(url, seed_list_dist));
    }
}

CrawledItem Frontier::front() {
    assert(!pq.empty());

    const UncrawledItem &front = pq.front();
    return CrawledItem(front.url, front.seed_list_dist, curr_urls[front.url]);
}

size_t Frontier::size() {
    return pq.size();
}

void Frontier::persist() {
    // TODO: persist to memory function

    string filename = "frontier_" + str(worker_id);
    FILE* file = fopen(filename, "wb");  // w = overwrite, b = binary

    auto it = pq.begin();

    while(it != pq.end()) {
        // write the UncrawledItem details to the doc
        // <url>\n<priority score (16 bits)> <distance from seed list (16 bits)> <times seen (32 bits)> \n
        it++;
    }
}