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
#include "../lib/consts.h"
#include <optional>

// struct WordPos {
//     size_t word_num; // 0-indexed for ordering of words in the query
//     size_t word_pos; // 0-indexed from start of page
// };

struct RankedPage {
    string url;
    int seed_list_dist;
    int pages_from_seed; // unsure if we have infra in place for this 
    int num_words_found_anchor;
    int num_words_found_title;
    int num_words_found_descr;
    int num_anchors;
    int times_seen;
    vector<vector<size_t>> word_positions;
    size_t doc_len;
    size_t description_len;
};

bool is_digit(char c) {
    return c >= '0' && c <= '9';
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
unordered_map<string, double> tldWeight = makeTldWeight(); // factory function to avoid having to implement initializer lists lol

double max(double i, double j) {
    if(i < j) {
        return j;
    } else {
        return i;
    }
}

// basically the same function from the frontier. Maybe should modify some of the constants or get rid of some features here
double calc_static_score(const string& u, int seed_list_dist) {
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
    string_view extension = (url.substr(start, len_ext));
    auto slot = tldWeight.find(extension);
    double factor_2 = (slot == tldWeight.end()) ? 0.6 : (*slot).value; 

    // points for closer to seed list

    const double e = 2.718;
    double factor_3 = max(double_pow(e, -0.04 * seed_list_dist), 0.4);  // NOTE: may need to tune the constant here

    // points for shortness of domain title

    double factor_4 = max((50.0 - domain_size) / 50.0, 0.5);

    // points for less subdomains

    double factor_5 = 1.0 / (1.0 + 0.1 * subdomain_count);

    // digit count in domain name hurts the score

    double factor_6 = 1.0 / (1.0 + 0.15 * digit_count_domain);

    // points for shortness of overall url

    double factor_7 = max((150.0 - url.size()) / 100.0, 0.5);

    double factor_8 = max(1.0 - 0.1 * path_depth, 0.4);

    double factor_9 = (qmarkfound) ? 0.75 : 1.0;
    
    return (factor_1 * factor_2 * factor_3 * factor_4 * factor_5 * factor_6 * factor_7 * factor_8 * factor_9);
}


double word_pos_score(const vector<vector<size_t>> &positions, int unique_words_in_query, size_t doc_len) {
    if (unique_words_in_query == 0 || doc_len == 0) return 0.0;

    double score = 0.0;

    // frequency of given unique words in the query
    for (int i = 0; i < unique_words_in_query; i++) {
        score += positions[i].size();
    }

    // Proximity score calculated 
    double proximity_score = 0.0;
    const double lambda = 1.0; // modify this factor during tuning

    for (int i = 0; i < unique_words_in_query; i++) {
        for (int j = i + 1; j < unique_words_in_query; j++) {

            for (size_t p : positions[i]) {
                size_t best_dist = SIZE_MAX;

                for (size_t q : positions[j]) {
                    size_t dist = (p > q) ? (p - q) : (q - p);
                    if (dist < best_dist) best_dist = dist;
                }

                if (best_dist != SIZE_MAX) {
                    proximity_score += 1.0 / (1.0 + best_dist);
                }
            }
        }
    }

    score += lambda * proximity_score;

    // Normalizing our length here using log
    double normalized = score / (1.0 + log(1.0 + doc_len));

    // Ensuring final factor is between 0.4-1.0
    if (normalized > 1.0) normalized = 1.0;
    if (normalized < 0.4) normalized = 0.4;

    return normalized;
}

// TODO
double calc_dynamic_score(RankedPage &r, size_t unique_words_in_query) {
    double factor_1 = word_pos_score(r.word_positions, r.doc_len, unique_words_in_query);

    return(factor_1);
}

struct RankedCompare {
    double dynamic_weight;
    size_t unique_words_in_query;

    RankedCompare(double f, size_t s) : dynamic_weight(f), unique_words_in_query(s) {}

    bool operator()(RankedPage a, RankedPage b) const {
        double a_score = calc_static_score(a.url, a.seed_list_dist) * (1-dynamic_weight) + calc_dynamic_score(b, unique_words_in_query) * dynamic_weight;
        double b_score = calc_static_score(b.url, b.seed_list_dist) * (1-dynamic_weight) + calc_dynamic_score(a, unique_words_in_query) * dynamic_weight;
        return a_score < b_score; 
    }
};

class Ranker {
private:
    priority_queue<RankedPage, vector<RankedPage>, RankedCompare> pq;
    double dynamic_weight;
    size_t unique_words_in_query;
    size_t size_lim;

public:
    Ranker(double dynamic_weight_init, size_t unique_words_in_query_init, size_t size_lim_init) : pq(RankedCompare(dynamic_weight_init, unique_words_in_query)), 
        dynamic_weight(dynamic_weight_init), unique_words_in_query(unique_words_in_query_init), size_lim(size_lim_init) { }

    void reset() {
        pq.clear();
    }

    vector<RankedPage> get_top_x(int x) {
        vector<RankedPage> v;
        for(int i = 0; i < x; i++) {
            v.push_back(pq.pop_move());
        }
        return v;
    }

    void rank(vector<RankedPage> v) {
        pq = priority_queue<RankedPage, vector<RankedPage>, RankedCompare>(
            v.begin(),
            v.end(),
            RankedCompare(dynamic_weight, unique_words_in_query),
            vector<RankedPage>()
        );
    }
};

