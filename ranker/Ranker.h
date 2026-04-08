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
#include "../lib/logger.h"
#include "ranker_consts.h"
#include <optional>

// struct WordPos {
//     size_t word_num; // 0-indexed for ordering of words in the query
//     size_t word_pos; // 1-indexed from start of page
// };

struct RankedPage {
    string url;
    string title;
    int seed_list_dist;
    int domains_from_seed; // unsure if we have infra in place for this 
    int num_unique_words_found_anchor;
    int num_unique_words_found_title;
    int num_unique_words_found_descr;
    int num_unique_words_found_url;
    int times_seen;
    vector<vector<size_t>> word_positions;
    size_t doc_len;
    size_t description_len;
};

struct LeanPage {
    string url;
    string title;
    double score; 
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

// basically the same function from the frontier. I have modified and added certain things to increase acuracy I think . . .
double calc_static_score(const RankedPage &p) {
    
    // domains from seed list is potentially more important that seed list distance itself 

    double factor_1 = max(double_pow(e, -0.08 * p.domains_from_seed), 0.2);  // NOTE: may need to tune the constant here

    string_view url = p.url.str_view(0, p.url.size());
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
    double factor_2 = (slot == tldWeight.end()) ? 0.2 : (*slot).value; 

    // points for closer to seed list
    double factor_3 = max(double_pow(e, -0.04 * p.seed_list_dist), 0.2);  // NOTE: may need to tune the constant here

    // points for shortness of domain title

    double factor_4 = max((50.0 - domain_size) / 50.0, 0.2);

    // points for less subdomains

    double factor_5 = 1.0 / (1.0 + 0.1 * subdomain_count);

    // digit count in domain name hurts the score

    double factor_6 = 1.0 / (1.0 + 0.15 * digit_count_domain);

    // points for shortness of overall url

    double factor_7 = max((150.0 - url.size()) / 100.0, 0.2);

    // penalizes the path depth of the link based on something like x.com/this/that/these/those

    double factor_8 = max(1.0 - 0.1 * path_depth, 0.2);

    // apparently a question mark in a link is kind of a bad thing? 

    double factor_9 = (qmarkfound) ? 0.0 : 1.0;
    
    return(((factor_1 * static_1_weight) + 
        (factor_2 * static_2_weight) + 
        (factor_3 * static_3_weight) + 
        (factor_4 * static_4_weight) + 
        (factor_5 * static_5_weight) +
        (factor_6 * static_6_weight) + 
        (factor_7 * static_7_weight) + 
        (factor_8 * static_8_weight) + 
        (factor_9 * static_9_weight)) / static_weight_sum);
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

double calc_dynamic_score(RankedPage &r, size_t unique_words_in_query) {
    // This factor checks frequency of unique words and proximity scores of the unique words to other unique words in the query 
    double factor_1 = word_pos_score(r.word_positions, r.doc_len, unique_words_in_query); // this score should be given extra weight in calculations

    // This factor scores based on number of times the link was seen during crawling
    double factor_2 = max(1.0 / (1.0 + double_pow(e, -k * (r.times_seen - n_0))), 0.2);

    // This factor scores based on how many unique words in the query were found in the title but penalized on length of the title
    double factor_3 = (r.num_unique_words_found_title / unique_words_in_query) * double_pow(e, (-Gamma_title * r.title.size()));

    // This factor scores based on how many unique words in the query were found in the description but penalized on length of the description
    double factor_4 = (r.num_unique_words_found_descr / unique_words_in_query) * double_pow(e, (-Gamma_desc * r.description_len));

    // This factor checks unique words in the query found in the anchor texts pointing to the link
    // PROBABLY WANT TO ADD MORE TO THIS FACTOR ONCE I KNOW MORE ABOUT ANCHOR TEXT
    double factor_5 = (r.num_unique_words_found_anchor / unique_words_in_query); 

    // This factor checks the URL for keywords in it
    double factor_6 = (r.num_unique_words_found_url / unique_words_in_query) * double_pow(e, (-Gamma_url * r.url.size()));

    // final score returned here with extra weightings 
    return(((factor_1 * factor_1_weight) + (factor_2 * factor_2_weight) + (factor_3 * factor_3_weight)
         + (factor_4 * factor_4_weight) + (factor_5 * factor_5_weight)) / dynamic_weight_sum);
}

// LeanPage input_total_score(RankedPage r, double dynamic_weight, size_t unique_words_in_query) {
//     double r_score = calc_static_score(r.url, r.seed_list_dist) * (1-dynamic_weight) + calc_dynamic_score(r, unique_words_in_query) * dynamic_weight;
//     return(LeanPage{std::move(r.url), std::move(r.title), r_score});
// }

struct RankedCompare {
    double dynamic_weight;
    size_t unique_words_in_query;

    RankedCompare(double f, size_t s) : dynamic_weight(f), unique_words_in_query(s) {}

    bool operator()(LeanPage a, LeanPage b) const {
        return a.score < b.score; 
    }
};

class Ranker {
private:
    priority_queue<LeanPage, vector<LeanPage>, RankedCompare> pq;
    double dynamic_weight;
    size_t unique_words_in_query;
    size_t size_lim;
    bool verbose_mode;

public:
    Ranker(double dynamic_weight_init, size_t unique_words_in_query_init, size_t size_lim_init, bool verbose_init = false) : pq(RankedCompare(dynamic_weight_init, unique_words_in_query)), 
        dynamic_weight(dynamic_weight_init), unique_words_in_query(unique_words_in_query_init), size_lim(size_lim_init), verbose_mode(verbose_init) { 
        
        if(verbose_mode) {
            logger::debug("Ranker is initialized with size limit of %zu, dynamic weighting of %f, and unique words in query of %zu", 
                size_lim_init, dynamic_weight_init, unique_words_in_query_init);
        }
    }

    void reset() {
        if(verbose_mode) {
            logger::debug("Ranker has cleared the pq and reset for next query");
        }
        pq.clear();
    }

    void change_words_in_query(size_t num_words) {
        unique_words_in_query = num_words;
        if(verbose_mode) {
            logger::debug("Ranker has been changed to serving an amount of %zu unique words in the query", num_words);
        }
    }

    vector<LeanPage> get_top_x(int x) {
        vector<LeanPage> v;
        for(int i = 0; i < x; i++) {
            v.push_back(pq.pop_move());
        }
        if(verbose_mode) {
            logger::debug("Ranker is returning vector of %zu elements", v.size());
        }
        return v;
    }

    // FOR HIGHEST EFFECTIVENESS, LIST SHOULD COME ORDERED BY SEED LIST TO ENSURE MOST PROMISING LINKS ARE SEEN IF THERE ARE SIZE CONSTRAINTS
    void rank(vector<RankedPage> v) {
        auto end_it = v.end();
        // limit the amount of pages being ranked 
        vector<LeanPage> input;
        for(size_t i = 0; i < v.size(); i++) {
            if(i >= size_lim) {
                end_it = v.begin() + i;
                break;
            } else {
                
                input.push_back(LeanPage{std::move(v[i].url), 
                    std::move(v[i].title), 
                    (calc_static_score(v[i]) * (1-dynamic_weight) + 
                    calc_dynamic_score(v[i], unique_words_in_query) * dynamic_weight)});
                if(verbose_mode) {
                    double r_score = calc_static_score(v[i]) * (1-dynamic_weight) + calc_dynamic_score(v[i], unique_words_in_query) * dynamic_weight;
                    logger::debug("The URL %s earned a score of: %.6f", v[i].url.data(), r_score);
                }
            }
        }
        pq = priority_queue<LeanPage, vector<LeanPage>, RankedCompare>(
            v.begin(),
            end_it,
            RankedCompare(dynamic_weight, unique_words_in_query),
            vector<LeanPage>()
        );
        if(verbose_mode) {
            logger::debug("Ranker has finished ranking %zu elements", pq.size());
        }
    }
};

