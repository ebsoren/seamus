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
#include "Frontier.h"

// TODO: Add this to lib instead so it is NOT an executable
// TODO: Create 5 queue buckets with given cutoffs instead of a PQ
// TODO: Store each bucket in a separate document and when we pull back, just take from the higest bucket with data present 
// TODO: Our goal is to clear out our top 3-4 buckets. Keep track of when we clear out the buckets to see the effectiveness 

unordered_map<string,double> makeTldWeight() {
    unordered_map<string, double> m(32);

    m.insert(string("gov"),1.2);
    m.insert(string("edu"),1.2);
    m.insert(string("mil"),1.2);
    m.insert(string("org"),1.1);
    m.insert(string("com"),1.0);
    m.insert(string("net"),1.0);
    m.insert(string("info"),0.8);
    m.insert(string("biz"),0.8);

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

double calcPriorityScore(const string& u, int seed_list_dist) {
    // points for http or https however https > http
    double factor_1;

    string_view url = u.str_view(0, u.size());

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

    int subdomain_count = 0;
    int digit_count_domain = 0;
    double domain_size = 0.0;
    string extension = string("");
    size_t start = 0;
    size_t len_ext = 0;
    for(int i = start_pos; i < url.size(); i++) {
        if(url[i] == '/' || url[i] == '?' || url[i] == '#' || url[i] == ':') {
            len_ext += 1;
            break;
        } else if(url[i] == '.') {
            subdomain_count++;
            extension = string("");
            start = i+1;
        } else {
            len_ext += 1;
            if(is_digit(url[i])) {
                digit_count_domain++;
            }
        }
        domain_size += 1.0;
    }
    string extension = (url.substr(start, len_ext)).to_string();
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


// UncrawledItem::UncrawledItem(string init_url, uint16_t init_seed_list_dist) : url(static_cast<string&&>(init_url)), seed_list_dist(init_seed_list_dist),
//     priority_score(calcPriorityScore(init_url, init_seed_list_dist)) { }


// CrawledItem::CrawledItem(string init_url, uint16_t init_seed_list_dist, uint16_t times_seen_init) : url(static_cast<string&&>(init_url)), seed_list_dist(init_seed_list_dist),
//     times_seen(times_seen_init) { }


bool UncrawledComp::operator()(const UncrawledItem& u1, const UncrawledItem& u2) const {
    return u1.priority_score < u2.priority_score;
}


// Frontier::Frontier(uint16_t worker_id_init, size_t initial_map_size = 2048, double initial_loading_factor = 0.65) 
//     : curr_urls(initial_map_size, initial_loading_factor), worker_id(worker_id_init) { }

void Frontier::push(const UncrawledItem &u) {
    uint32_t& count = curr_urls[u.url];
    
    if (count++ == 0) {
        pq.push(u);
    }
}

void Frontier::push(string &url, int seed_list_dist) {
    uint32_t& count = curr_urls[url];
    
    if (count++ == 0) {
        pq.push(UncrawledItem(static_cast<string&&>(url), seed_list_dist));
    }
}

void Frontier::pop() {
    if(pq.size() != 0) {
        pq.pop();
    } 
}

CrawledItem Frontier::front() {
    assert(!pq.empty());

    UncrawledItem front =
    static_cast<UncrawledItem&&>(
        const_cast<UncrawledItem&>(pq.front())
    );

    return CrawledItem(static_cast<string&&>(front.url), front.seed_list_dist, curr_urls[front.url]);
}

size_t Frontier::size() {
    return pq.size();
}

void Frontier::persist() {
        // Create a file (if it already exists, fail -- don't want to overwrite)
    string path = string::join("frontier_", string(worker_id), ".txt");
    FILE* fd = fopen(path.data(), "wx");

    if (fd == nullptr) perror("Error opening frontier file for writing.");

    // <url_len (32 bits)><url (variable)><distance from seed list (16 bits)><times seen (32 bits)>
    uint64_t total_bytes = 0;
    for (auto it = pq.begin(); it != pq.end(); ++it) {
        total_bytes += (*it).url.size() + 10;
    }

    // Write the size
    fwrite(&total_bytes, sizeof(total_bytes), 1, fd);
    fwrite("\n", sizeof(char), 1, fd);

    // Write the file
    for (auto it = pq.begin(); it != pq.end(); ++it) {
        string url = (*it).url;
        uint16_t seed_dist = (*it).seed_list_dist;
        uint32_t times_seen = curr_urls[url];
        uint32_t sz = url.size();
        fwrite(&sz, sizeof(uint32_t), 1, fd);
        fwrite(url.data(), sizeof(char), url.size(), fd);
        fwrite(&seed_dist, sizeof(uint16_t), 1, fd);
        fwrite(&times_seen, sizeof(uint32_t), 1, fd);
    }

    fclose(fd);
}