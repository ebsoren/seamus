#pragma once

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
#include "../query/expressions.h"
#include "ranker_consts.h"
#include <optional>


template <typename T>
void reverse(vector<T>& vec) {
    size_t n = vec.size();
    for (size_t i = 0; i < n / 2; ++i) {
        // Swap element i with its mirror at the end
        T temp = std::move(vec[i]);
        vec[i] = std::move(vec[n - 1 - i]);
        vec[n - 1 - i] = std::move(temp);
    }
}

struct RankedPage {
    string url = string("");
    string title = string("");
    int seed_list_dist;
    int domains_from_seed; // unsure if we have infra in place for this 
    int num_unique_words_found_anchor;
    int num_unique_words_found_title;
    int num_unique_words_found_descr; // cut
    int num_unique_words_found_url;
    int times_seen;
    vector<vector<size_t>> word_positions;
    size_t doc_len; // ask bout ts
    size_t description_len; // cut
};

struct LeanPage {
    string url = string("");
    string title = string("");
    double score; 
};

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}


// These are all the allowed endings that we can rank
unordered_map<string,double> makeTldWeight() {
    unordered_map<string, double> m(32);

    m.insert(string("gov"),1.2);
    m.insert(string("edu"),1.2);
    m.insert(string("mil"),1.2);
    m.insert(string("org"),1.1);
    m.insert(string("wiki"),1.1);
    m.insert(string("news"),1.1);
    m.insert(string("pro"),1.1);
    m.insert(string("museum"),1.1);
    m.insert(string("jobs"),1.1);
    m.insert(string("page"),1.1);
    m.insert(string("blog"),1.1);
    m.insert(string("info"),1.1);
    m.insert(string("science"),1.1);
    m.insert(string("health"),1.1);
    m.insert(string("media"),1.1);
    m.insert(string("com"),1.0);
    m.insert(string("int"),1.0);
    m.insert(string("net"),1.0);
    m.insert(string("io"),1.0);
    m.insert(string("dev"),1.0);
    m.insert(string("app"),1.0);
    m.insert(string("ai"),1.0);
    m.insert(string("co"),1.0);
    m.insert(string("cc"),1.0);
    m.insert(string("tv"),1.0);
    m.insert(string("me"),1.0);
    m.insert(string("fm"),1.0);
    m.insert(string("ly"),1.0);
    m.insert(string("gg"),1.0);
    m.insert(string("sh"),1.0);
    m.insert(string("to"),1.0);
    m.insert(string("xyz"),1.0);
    m.insert(string("tech"),1.0);
    m.insert(string("cloud"),1.0);
    m.insert(string("so"),1.0);
    m.insert(string("gl"),1.0);
    m.insert(string("is"),1.0);
    m.insert(string("ws"),1.0);
    m.insert(string("ac"),1.0);
    m.insert(string("uk"),1.0);
    m.insert(string("us"),1.0);
    m.insert(string("au"),1.0);
    m.insert(string("ca"),1.0);
    m.insert(string("nz"),1.0);
    m.insert(string("ie"),1.0);
    m.insert(string("za"),1.0);
    m.insert(string("sg"),1.0);
    m.insert(string("hk"),1.0);
    m.insert(string("in"),1.0);

    return m;
}

vector<double> get_word_probabilities(vector<string> &s) {
    // TODO: THIS SHOULD RETURN THE COMMONNESS OF THE WORDS GIVEN WHAT WE'VE SEEN IN SEARCHING
    vector<double> probs(s.size());
    for(size_t i = 0; i < s.size(); i++) {
        // probs[i] = find_probability(s[i]);
    }
    return probs;
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

template <typename T>
T min_element(vector<T> &v) {
    T min_elt = v[0];
    for(int i = 1; i < v.size(); i++) {
        if(v[i] < min_elt) {
            min_elt = v[i];
        }
    }
    return min_elt;
}
template <typename T>
T max_element(vector<T> &v) {
    T max_elt = v[0];
    for(int i = 1; i < v.size(); i++) {
        if(v[i] > min_elt) {
            max_elt = v[i];
        }
    }
    return max_elt;
}

double word_rarity_freq_score(vector<double> probs, vector<int> counts) {

    int n = probs.size();
    vector<double> x(n);

    // log rarity
    for (int i = 0; i < n; i++) {
        x[i] = -log(probs[i]);
    }

    double xmin = min_element(x);
    double xmax = max_element(x);

    // weights in [0.2, 1]
    std::vector<double> w(n);
    for (int i = 0; i < n; i++) {
        if (xmax == xmin) {
            w[i] = 1.0;
        } else {
            double z = (x[i] - xmin) / (xmax - xmin);
            w[i] = 0.2 + 0.8 * z;
        }
    }

    // combine for final score
    double lambda = 0.5;
    double raw = 0.0, max_possible = 0.0;

    for (int i = 0; i < n; i++) {
        double f = 1 - exp(-lambda * counts[i]);
        raw += w[i] * f;
        max_possible += w[i];
    }

    double S = (max_possible > 0) ? raw / max_possible : 0.0;

    // scale to [0.2, 1]
    double final_score = 0.2 + 0.8 * S;
    
    return final_score;
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

double calc_dynamic_score(RankedPage &r, vector<string> &unique_query_words) {
    // This factor checks frequency of unique words and proximity scores of the unique words to other unique words in the query 
    double factor_1 = word_pos_score(r.word_positions, r.doc_len, unique_query_words.size()); // this score should be given extra weight in calculations

    // This factor scores based on number of times the link was seen during crawling
    double factor_2 = max(1.0 / (1.0 + double_pow(e, -k * (r.times_seen - n_0))), 0.2);

    // This factor scores based on how many unique words in the query were found in the title but penalized on length of the title
    double factor_3 = (r.num_unique_words_found_title / unique_query_words.size()) * double_pow(e, (-Gamma_title * r.title.size()));

    // This factor scores based on how many unique words in the query were found in the description but penalized on length of the description
    double factor_4 = (r.num_unique_words_found_descr / unique_query_words.size()) * double_pow(e, (-Gamma_desc * r.description_len));

    // This factor checks unique words in the query found in the anchor texts pointing to the link
    // PROBABLY WANT TO ADD MORE TO THIS FACTOR ONCE I KNOW MORE ABOUT ANCHOR TEXT
    double factor_5 = (r.num_unique_words_found_anchor / unique_query_words.size()); 

    // This factor checks the URL for keywords in it
    double factor_6 = (r.num_unique_words_found_url / unique_query_words.size()) * double_pow(e, (-Gamma_url * r.url.size()));

    // This factor scores based on the rarity of each word combined with its frequency
    vector<int> counts(r.word_positions.size());
    for(size_t i = 0; i < r.word_positions.size(); i++) {
        counts[i] = r.word_positions[i].size();
    }
    double factor_6 = word_rarity_freq_score(get_word_probabilities(unique_query_words), counts);

    // final score returned here with extra weightings 
    return(((factor_1 * factor_1_weight) + (factor_2 * factor_2_weight) + (factor_3 * factor_3_weight)
         + (factor_4 * factor_4_weight) + (factor_5 * factor_5_weight) + (factor_6 * factor_6_weight)) / dynamic_weight_sum);
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
        return a.score > b.score; 
    }
};

class Ranker {
private:
    priority_queue<LeanPage, vector<LeanPage>, RankedCompare> pq;
    double dynamic_weight;
    vector<string> unique_query_words; 
    int num_pages_returned;
    bool verbose_mode;

public:
    Ranker(int num_pages_returned_init = RANKED_ON_EACH, double dynamic_weight_init = DEFAULT_DYNAMIC_WEIGHT, bool verbose_init = false) : 
        num_pages_returned(num_pages_returned_init), dynamic_weight(dynamic_weight_init), verbose_mode(verbose_init) { 
        
        if(verbose_mode) {
            logger::debug("Ranker is initialized with num pages returned of %d, and dynamic weighting of %f", 
                num_pages_returned_init, dynamic_weight_init);
        }
    }

    void reset() {
        if(verbose_mode) {
            logger::debug("Ranker has cleared the vector and reset for next query");
        }
        pq.clear();
    }

    void set_new_query(vector<string> s) {
        unique_query_words = std::move(s);
        if(verbose_mode) {
            logger::debug("Ranker has been changed to serving an amount of %zu unique words in the query", s.size());
        }
        pq = priority_queue<LeanPage, vector<LeanPage>, RankedCompare>(
            RankedCompare(dynamic_weight, unique_query_words.size()),
            vector<LeanPage>()
        );
    }

    // FOR HIGHEST EFFECTIVENESS, LIST SHOULD COME ORDERED BY SEED LIST TO ENSURE MOST PROMISING LINKS ARE SEEN IF THERE ARE SIZE CONSTRAINTS
    vector<LeanPage> rank(vector<RankedPage> v) {
        //initialize the pq
        pq = priority_queue<LeanPage, vector<LeanPage>, RankedCompare>(
            RankedCompare(dynamic_weight, unique_query_words.size()),
            vector<LeanPage>()
        );

        // limit the amount of pages being ranked 
        vector<LeanPage> input;
        for(size_t i = 0; i < v.size(); i++) {
            // calculate the score of the page
            double r_score = calc_static_score(v[i]) * (1-dynamic_weight) + calc_dynamic_score(v[i], unique_query_words) * dynamic_weight;

            if(pq.size() < num_pages_returned) {
                pq.push(LeanPage{std::move(v[i].url), 
                    std::move(v[i].title), 
                    r_score});
            } else {
                
                if(verbose_mode) {
                    logger::debug("The URL %s earned a score of: %.6f", v[i].url.data(), r_score);
                }
                if(r_score > pq.front().score) {
                    pq.push(LeanPage{std::move(v[i].url), 
                        std::move(v[i].title), 
                        r_score});
                    pq.pop();
                }
            }
        }
        
        if(verbose_mode) {
            logger::debug("Ranker has finished ranking %zu elements", pq.size());
        }

        vector<LeanPage> results;
        for(int i = 0; i < num_pages_returned; i++) {
            results.push_back(pq.pop_move());
        }
        if(verbose_mode) {
            logger::debug("Ranker is returning vector of %zu elements", results.size());
        }

        // reverse results so that the highest ranked is first in the ordering
        reverse(results);

        return(results);
    }
};

