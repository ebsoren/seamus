#include <cstddef>    // for size_t
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <stdio.h>
#include <cstring>
#include "vector.h"
#include "string.h"
#include "unordered_map.h"
#include "priority_queue.h"
#include "../url_store/url_store.h"
#include "utils.h"
#include "consts.h"

size_t get_priority_bucket(const string& url, int seed_list_dist) {
    // TODO(Erik): write this function
    // 0 is the index of the highest priority bucket, PRIORITY_BUCKETS - 1 is the index of the lowest priority bucket
    // PRIORITY_BUCKETS defined in ~/lib/consts.h

    // CURRENT FUNCTION WRITTEN WITH 8 EXPECTED BUCKETS, WILL NEED TO CHANGE IF THAT IS CHANGED!!
    int score = calcPriorityScore(url, seed_list_dist);

    if      (score >= 400000) return 0;  // elite
    if      (score >= 300000) return 1;
    if      (score >= 200000) return 2;
    if      (score >= 100000) return 3;
    if      (score >= 50000) return 4;
    if      (score >= 40000) return 5;
    if      (score >= 30000) return 6;
    if      (score >= 20000) return 7;
    else                     return 8;  // don't add to the buckets (bad url)
}

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
const unordered_map<string,double> tldWeight = makeTldWeight(); // factory function to avoid having to implement initializer lists lol

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

double max(double i, double j) {
    if(i < j) {
        return j;
    } else {
        return i;
    }
}

int calcPriorityScore(const string& u, int seed_list_dist) {
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
    int path_depth = 0;
    bool qmarkfound = false;
    size_t start = 0;
    size_t len_ext = 0;
    for(int i = start_pos; i < url.size(); i++) {
        if(url[i] == '/' || url[i] == '?' || url[i] == '#' || url[i] == ':') {
            len_ext += 1;
            while(i < url.size()) {
                if(url[i] == '/') { 
                    path_depth++;
                } else if(url[i] == '?') {
                    qmarkfound = true;
                }
                i++;
            }
            break;
        } else if(url[i] == '.') {
            subdomain_count++;
            start = i+1;
            len_ext = 0;
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
    double factor_3 = max(double_pow(e, -0.04 * seed_list_dist), 0.4);  // NOTE: may need to tune the constant here

    // points for shortness of domain title

    double factor_4 = max((50.0 - domain_size) / 50.0, 0.5);

    // points for less subdomains

    double factor_5 = 1.0 / (1.0 + 0.1 * subdomain_count);

    // digit count in domain name hurts the score

    double factor_6 = 1.0 / (1.0 + 0.15 * digit_count_domain);

    // points for shortness of overall url

    double factor_7 = max((150.0 - url.size()) / 100.0, 0.5);
    
    return int((factor_1 * factor_2 * factor_3 * factor_4 * factor_5 * factor_6 * factor_7) * 10000.0);
}

struct UncrawledItem {
    string url;
    uint16_t seed_list_dist;

    UncrawledItem(string init_url, uint16_t init_seed_list_dist) : url(static_cast<string&&>(init_url)), seed_list_dist(init_seed_list_dist) { }

};

struct CrawledItem {
    string url;
    uint16_t seed_list_dist;

    CrawledItem(string init_url, uint16_t init_seed_list_dist) : url(static_cast<string&&>(init_url)), seed_list_dist(init_seed_list_dist) { }
};

struct UncrawledComp {
    bool operator()(const UncrawledItem& u1, const UncrawledItem& u2) const;
};

class Frontier {
private:
    priority_queue<UncrawledItem, vector<UncrawledItem>, UncrawledComp> pq;
    vector<vector<UncrawledItem>> priority_buckets;
    // unordered_map<string, uint32_t> curr_urls;
    uint16_t worker_id;
public:
    Frontier(u_int16_t worker_id_init) 
        : worker_id(worker_id_init), priority_buckets(PRIORITY_BUCKETS) { }

    void Frontier::push(const UncrawledItem &u) {
        size_t bucket = get_priority_bucket(u.url, u.seed_list_dist);
        for(size_t i = bucket; i < PRIORITY_BUCKETS; i++) {
            if(priority_buckets[i].size() < MAX_SIZE_BUCKET) {
                priority_buckets[i].push_back(u);
                return;
            }
        }
    }

    void Frontier::push(string &&url, int seed_list_dist) {
        size_t bucket = get_priority_bucket(url, seed_list_dist);
        for(size_t i = bucket; i < PRIORITY_BUCKETS; i++) {
            if(priority_buckets[i].size() < MAX_SIZE_BUCKET) {
                priority_buckets[i].push_back(UncrawledItem(static_cast<string&&>(url), seed_list_dist));
                return;
            }
        }
    }

    void Frontier::pop() {
        for(size_t i = 0; i < PRIORITY_BUCKETS; i++) {
            if(!priority_buckets[i].empty()) {
                priority_buckets[i].pop_back();
                return;
            }
        }
    }

    CrawledItem Frontier::front() {
        for(size_t i = 0; i < PRIORITY_BUCKETS; i++) {
            size_t idx = priority_buckets[i].size();
            if(idx != 0) {
                UncrawledItem& item = priority_buckets[i][idx-1];
                return CrawledItem(
                    (item.url.str_view(0,item.url.size()).to_string()),
                    item.seed_list_dist
                );
            }
        }
        throw std::runtime_error("Frontier empty");
        return;
    }

    size_t Frontier::size() {
        size_t sz = 0;
        for(size_t i = 0; i < PRIORITY_BUCKETS; i++) {
            sz += priority_buckets[i].size();
        }
        return sz;
    }

    void Frontier::persist() {
            // Create a file (if it already exists, fail -- don't want to overwrite)
        string path = string::join("frontier_", string(worker_id), ".txt");
        FILE* fd = fopen(path.data(), "wx");

        if (fd == nullptr) perror("Error opening frontier file for writing.");

        // <url_len (32 bits)><url (variable)><distance from seed list (16 bits)>
        uint64_t total_bytes = 0;
        for(size_t i = 0; i < PRIORITY_BUCKETS; i++) {
            for(auto it = priority_buckets[i].begin(); it != priority_buckets[i].end(); it++) {
                total_bytes += (*it).url.size();
            }
        }

        // Write the size
        fwrite(&total_bytes, sizeof(total_bytes), 1, fd);
        fwrite("\n", sizeof(char), 1, fd);

        // write the file
        for(uint16_t i = 0; i < PRIORITY_BUCKETS; i++) {
            fwrite(&i, sizeof(uint16_t), 1, fd);
            uint64_t bucket_size = priority_buckets[i].size();
            fwrite(&bucket_size, sizeof(uint64_t), 1, fd);
            for(auto it = priority_buckets[i].begin(); it != priority_buckets[i].end(); it++) {
                string url = static_cast<string&&>((*it).url);
                uint16_t seed_dist = (*it).seed_list_dist;
                uint32_t sz = url.size();
                fwrite(&sz, sizeof(uint32_t), 1, fd);
                fwrite(url.data(), sizeof(char), url.size(), fd);
                fwrite(&seed_dist, sizeof(uint16_t), 1, fd);
            }
        }

        fclose(fd);
    }

};