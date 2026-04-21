#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <stdio.h>
#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/unordered_map.h"
#include "../lib/priority_queue.h"
#include "../lib/utils.h"
#include "../lib/consts.h"
#include "../lib/logger.h"
#include "../lib/rpc_query_handler.h"
#include "../lib/chunk_manager_query.h"
#include "../query/expressions.h"
#include "../lib/rpc_query_handler.h"
#include "../url_store/url_store.h"
#include "ranker_consts.h"
#include <optional>


// Case-insensitive substring check restricted to a URL's host portion
// (between "://" and the first '/', '?', '#', or ':'). Used to gate the
// tracked-URL log lines so they don't fire on "wikipedia" appearing in
// a path or query string.
inline bool url_host_contains_ci(const string& url, const char* needle) {
    const char* d = url.data();
    size_t sz = url.size();
    size_t i = 0;
    while (i + 2 < sz && d[i] != ':') i++;
    size_t dom_start = (i + 2 < sz && d[i] == ':' && d[i+1] == '/' && d[i+2] == '/') ? i + 3 : 0;
    size_t dom_end = dom_start;
    while (dom_end < sz) {
        char c = d[dom_end];
        if (c == '/' || c == '?' || c == '#' || c == ':') break;
        dom_end++;
    }
    size_t nlen = 0;
    while (needle[nlen] != '\0') nlen++;
    if (nlen == 0 || dom_end <= dom_start || nlen > dom_end - dom_start) return false;
    for (size_t j = dom_start; j + nlen <= dom_end; j++) {
        bool ok = true;
        for (size_t k = 0; k < nlen; k++) {
            char hc = d[j+k]; if (hc >= 'A' && hc <= 'Z') hc += 32;
            char nc = needle[k]; if (nc >= 'A' && nc <= 'Z') nc += 32;
            if (hc != nc) { ok = false; break; }
        }
        if (ok) return true;
    }
    return false;
}

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

// "two words"
// for phrase words in word_positions, simply store the location of the start of the phrase
struct RankedPage {
    string url = string("");
    string title = string("");
    int seed_list_dist;
    int domains_from_seed; // unsure if we have infra in place for this 
    // int num_unique_words_found_anchor; // number of unique words in the query found
    // int num_unique_words_found_title;
    // int num_unique_words_found_url;
    int unique_phrases_matched_anchor;
    int total_link_frequency_anchor;
    vector<bool> anchor_words_found;
    vector<bool> title_words_found;
    vector<bool> url_words_found;
    int times_seen;
    vector<vector<size_t>> word_positions; // this is just anywhere on the doc page
    size_t doc_len; // max for this is 100 KB
};

struct RankerNodeInfo {
    string node; // can be single or multi-word
    size_t cnt;
    bool is_phrase;
};

inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}


// These are all the allowed endings that we can rank
inline unordered_map<string,double> makeTldWeight() {
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
inline unordered_map<string, double> tldWeight = makeTldWeight(); // factory function to avoid having to implement initializer lists lol

inline double max(double i, double j) {
    if(i < j) {
        return j;
    } else {
        return i;
    }
}

inline double min(double i, double j) {
    if(i < j) {
        return i;
    } else {
        return j;
    }
}

// basically the same function from the frontier. I have modified and added certain things to increase acuracy I think . . .
inline double calc_static_score(const RankedPage &p, bool verbose = false) {
    
    // domains from seed list is potentially more important that seed list distance itself 
    double factor_1 = max(exp(-0.08 * (p.domains_from_seed + 1)), 0.05);  // NOTE: may need to tune the constant here

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
    
    for(size_t i = start_pos; i < url.size(); i++) {
        if(url[i] == '/' || url[i] == '?' || url[i] == '#' || url[i] == ':') {
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
    double factor_3 = max(exp(factor_3_const * (p.seed_list_dist+1)), 0.05);  // NOTE: may need to tune the constant here

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
    
    // Calculate final score
    double final_score = ((factor_1 * static_1_weight) + 
                          (factor_2 * static_2_weight) + 
                          (factor_3 * static_3_weight) + 
                          (factor_4 * static_4_weight) + 
                          (factor_5 * static_5_weight) +
                          (factor_6 * static_6_weight) + 
                          (factor_7 * static_7_weight) + 
                          (factor_8 * static_8_weight) + 
                          (factor_9 * static_9_weight)) 
                          / static_weight_sum;

    if (verbose) {
        printf("\n--- STATIC SCORING [%.*s] ---\n", (int)p.url.size(), p.url.data());
        printf("F1 (Domain Dist) : %.6f * %.2f = %.6f\n", factor_1, static_1_weight, factor_1 * static_1_weight);
        printf("F2 (TLD Weight)  : %.6f * %.2f = %.6f\n", factor_2, static_2_weight, factor_2 * static_2_weight);
        printf("F3 (Seed Dist)   : %.6f * %.2f = %.6f\n", factor_3, static_3_weight, factor_3 * static_3_weight);
        printf("F4 (Domain Len)  : %.6f * %.2f = %.6f\n", factor_4, static_4_weight, factor_4 * static_4_weight);
        printf("F5 (Subdomains)  : %.6f * %.2f = %.6f\n", factor_5, static_5_weight, factor_5 * static_5_weight);
        printf("F6 (Domain Digits): %.6f * %.2f = %.6f\n", factor_6, static_6_weight, factor_6 * static_6_weight);
        printf("F7 (URL Length)  : %.6f * %.2f = %.6f\n", factor_7, static_7_weight, factor_7 * static_7_weight);
        printf("F8 (Path Depth)  : %.6f * %.2f = %.6f\n", factor_8, static_8_weight, factor_8 * static_8_weight);
        printf("F9 (No Queries)  : %.6f * %.2f = %.6f\n", factor_9, static_9_weight, factor_9 * static_9_weight);
        printf("FINAL STATIC SCORE: %.6f\n", final_score);
        printf("--------------------------------------\n");
        fflush(stdout);
    }

    return final_score;
}

template <typename T>
T min_element(vector<T> &v) {
    if (v.size() == 0) return T(0);  // safe fallback
    T min_elt = v[0];
    for(size_t i = 1; i < v.size(); i++) {
        if(v[i] < min_elt) {
            min_elt = v[i];
        }
    }
    return min_elt;
}

template <typename T>
T max_element(vector<T> &v) {
    if (v.size() == 0) return T(0);  // safe fallback
    T max_elt = v[0];
    for(size_t i = 1; i < v.size(); i++) {
        if(v[i] > max_elt) {
            max_elt = v[i];
        }
    }
    return max_elt;
}

inline double bm25_score(const vector<vector<size_t>>& pos, size_t dl, const vector<uint64_t>& tf, size_t N) { 
    if (dl == 0 || pos.size() == 0 || tf.size() == 0 || N == 0)
        return 0.0;

    double score = 0.0;

    size_t safe_n = min(pos.size(), tf.size());

    for (size_t i = 0; i < safe_n; i++) { 
        double t = pos[i].size(); 
        if (t == 0) continue; 

        double df = tf[i];
        if (df <= 0) continue;

        double idf = log((N - df + 0.5)/(df + 0.5) + 1.0); 
        double norm = (t*(1.2+1))/(t + 1.2*(1 - 0.75 + 0.75*(dl/500.0))); 
        score += idf * norm; 
    } 

    return score; 
}

inline double phrase_bonus(const vector<vector<size_t>>& pos) { 
    if (pos.size() < 2) {
        return 0.0;
    }
    int hits = 0;
    
    for (size_t i = 0; i+1 < pos.size(); i++) { 
        for (auto a : pos[i]) { 
            for (auto b : pos[i+1]) { 
                if (b == a+1) { 
                    hits++; 
                    break; 
                } 
            } 
        } 
    } 
    return (double)hits / (pos.size()-1); 
}


inline double word_pos_score(const vector<vector<size_t>> &positions, size_t doc_len, int unique_words_in_query) {
    if (unique_words_in_query == 0 || doc_len == 0 || positions.size() == 0)
        return 0.0;

    if (positions.size() < (size_t)unique_words_in_query)
        return 0.0;

    double proximity_score = 0.0;
    double max_possible_prox = 0.0;

    const size_t SENTENCE_WINDOW = 20;

    for (int i = 0; i < unique_words_in_query; i++) {
        for (int j = i + 1; j < unique_words_in_query; j++) {

            if (i >= (int)positions.size() || j >= (int)positions.size())
                continue;

            const auto& posA = positions[i];
            const auto& posB = positions[j];

            if (posA.size() == 0 || posB.size() == 0)
                continue;

            double max_pair_occurrences = (double)min(posA.size(), posB.size());
            double perfect_dist = (double)(j - i);

            if (perfect_dist == 0) continue;

            max_possible_prox += log(1.0 + (max_pair_occurrences / perfect_dist));

            size_t b = 0;
            double current_pair_score = 0.0;

            for (size_t a = 0; a < posA.size(); ++a) {
                size_t p = posA[a];

                while (b + 1 < posB.size()) {
                    size_t current_dist = abs((long)p - (long)posB[b]);
                    size_t next_dist = abs((long)p - (long)posB[b+1]);

                    if (next_dist <= current_dist) b++;
                    else break;
                }

                size_t best_dist = abs((long)p - (long)posB[b]);

                if (best_dist <= SENTENCE_WINDOW && best_dist > 0) {
                    current_pair_score += (1.0 / (double)best_dist);
                }
            }

            proximity_score += log(1.0 + current_pair_score);
        }
    }

    if (max_possible_prox <= 0.0) return 0.0;

    double normalized_prox = proximity_score / max_possible_prox;

    if (std::isnan(normalized_prox) || std::isinf(normalized_prox))
        return 0.0;

    double final_score = LAMBDA_POS * normalized_prox;

    if (final_score > 1.0) final_score = 1.0;
    if (final_score < 0.0) final_score = 0.0;

    return final_score;
}

// LeanPage input_total_score(RankedPage r, double dynamic_weight, size_t unique_words_in_query) {
//     double r_score = calc_static_score(r.url, r.seed_list_dist) * (1-dynamic_weight) + calc_dynamic_score(r, unique_words_in_query) * dynamic_weight;
//     return(LeanPage{std::move(r.url), std::move(r.title), r_score});
// }

struct RankedCompare {
    double dynamic_weight;
    size_t unique_words_in_query;

    RankedCompare(double f = DEFAULT_DYNAMIC_WEIGHT, size_t s = 10) : dynamic_weight(f), unique_words_in_query(s) {}

    bool operator()(const LeanPage& a, const LeanPage& b) const {
        return a.score > b.score; 
    }
};

struct QueryInfo {
    string phrase;
    uint64_t freq;
    bool is_phrase;

    QueryInfo& operator=(const QueryInfo& other) {
        if (this != &other) {
            phrase = string(other.phrase.data(), other.phrase.size());
            freq = other.freq;
            is_phrase = other.is_phrase;
        }
        return *this;
    }

    QueryInfo(const QueryInfo& other)
        : phrase(other.phrase.data(), other.phrase.size()),
        freq(other.freq),
        is_phrase(other.is_phrase) {}

    QueryInfo(string phrase = string(""), uint64_t freq = 0, bool is_phrase = false)
        : phrase(phrase.data(), phrase.size()), freq(freq), is_phrase(is_phrase) {} 
};


class SmallPQ {
private:
    vector<LeanPage> v;
    size_t max_size;

public:
    SmallPQ(size_t max_size_init = RANKED_ON_EACH) : max_size(max_size_init) { }

    void push(LeanPage l) {
        
        if (v.size() == max_size && l.score <= v.back().score) {
            return;
        }

        size_t insert_pos = 0;
        while (insert_pos < v.size() && v[insert_pos].score > l.score) {
            insert_pos++;
        }

        v.insert(insert_pos, std::move(l));

        if (v.size() > max_size) {
            v.pop_back();
        }
    }

    vector<LeanPage> getResults() {
        return std::move(v);
    }
};

class Ranker {
public:
    // Global accumulators across all chunks for a single query.
    // Reset before fan-out, read after join.
    static inline std::atomic<size_t> s_miss_count{0};
    static inline std::atomic<size_t> s_total_count{0};

    static void reset_stats() {
        s_miss_count.store(0, std::memory_order_relaxed);
        s_total_count.store(0, std::memory_order_relaxed);
    }

    static void print_stats() {
        size_t misses = s_miss_count.load(std::memory_order_relaxed);
        size_t total  = s_total_count.load(std::memory_order_relaxed);
        double pct = total > 0 ? 100.0 * misses / total : 0.0;
        logger::error("[RANKER] GLOBAL: %zu/%zu docs missed urlstore (%.1f%%)",
                misses, total, pct);
    }

private:
    SmallPQ pq; 
    double dynamic_weight;
    vector<QueryInfo> unique_query_terms; 
    QueryInfo rarestTerm;
    int num_pages_returned;
    bool verbose_mode;
    bool is_query_set;
    UrlStore* url_store;

    double word_rarity_freq_score(RankedPage &r) {

        int n = min((int)unique_query_terms.size(), (int)r.word_positions.size());
        if (n == 0) return 0.0;

        vector<double> x(n);

        for (int i = 0; i < n; i++) {
            x[i] = -log(1.0 + (double)unique_query_terms[i].freq);
        }

        double xmin = min_element(x);
        double xmax = max_element(x);

        vector<double> w(n);
        for (int i = 0; i < n; i++) {
            if (xmax == xmin) {
                w[i] = 1.0;
            } else {
                double z = (x[i] - xmin) / (xmax - xmin);
                w[i] = 0.2 + 0.8 * z;
            }
        }

        double raw = 0.0, max_possible = 0.0;

        for (int i = 0; i < n; i++) {
            double f = 1 - exp(-LAMBDA_FREQ * (double)r.word_positions[i].size());
            raw += w[i] * f;
            max_possible += w[i];
        }

        if (max_possible <= 0.0) return 0.0;

        double S = raw / max_possible;

        if (std::isnan(S) || std::isinf(S)) return 0.0;

        return 0.1 + 0.9 * S;
    }

    inline double early_position_score(
        const vector<vector<size_t>>& positions,
        size_t doc_len,
        const vector<QueryInfo>& terms
    ) {
        // Safety checks
        if (doc_len == 0 || positions.size() == 0 || terms.size() == 0)
            return 0.0;

        size_t n = positions.size();
        if (terms.size() < n) n = terms.size();

        double weighted_score = 0.0;
        double total_weight = 0.0;

        for (size_t i = 0; i < n; i++) {
            const auto& pos_list = positions[i];

            if (pos_list.size() == 0)
                continue;

            // RARITY WEIGHT
            double rarity = 1.0 / (1.0 + log(1.0 + (double)terms[i].freq));

            size_t earliest = pos_list[0];
            for (size_t j = 1; j < pos_list.size(); j++) {
                if (pos_list[j] < earliest)
                    earliest = pos_list[j];
            }

            if (doc_len == 0) continue;

            // NORMALIZE
            // 1.0 = very early, 0.0 = very late
            double pos_score = 1.0 - ((double)earliest / (double)doc_len);

            // Clamp safety
            if (pos_score < 0.0) pos_score = 0.0;
            if (pos_score > 1.0) pos_score = 1.0;

            weighted_score += rarity * pos_score;
            total_weight += rarity;
        }

        if (total_weight <= 0.0)
            return 0.0;

        double final_score = weighted_score / total_weight;

        return final_score;
    }

    double calc_dynamic_score(RankedPage &r) {

        if (r.word_positions.size() == 0) return 0.0;
        if (unique_query_terms.size() == 0) return 0.0;

        // This factor checks frequency of unique words and proximity scores of the unique words to other unique words in the query 
        double factor_1 = word_pos_score(r.word_positions, r.doc_len, unique_query_terms.size()); // this score should be given extra weight in calculations

        // This factor scores based on number of times the link was seen during crawling
        double factor_2 = max(1.0 / (1.0 + exp(-k * (r.times_seen - n_0))), 0.0);

        // PRE-CALCULATE RARITY WEIGHTS FOR FACTORS 3 & 5
        double total_rarity_weight = 0.0;
        double matched_title_weight = 0.0;
        double matched_url_weight = 0.0;

        size_t safe_n = min(unique_query_terms.size(), r.title_words_found.size());
        safe_n = min(safe_n, r.url_words_found.size());

        for (size_t i = 0; i < safe_n; ++i) {
            // Rarer words (low freq) yield high weights. Common words (high freq) get squashed.
            double rarity_weight = 1.0 / (1.0 + log(1.0 + (double)unique_query_terms[i].freq));
            total_rarity_weight += rarity_weight;

            if (r.title_words_found[i]) {
                matched_title_weight += rarity_weight;
            }
            if (r.url_words_found[i]) {
                matched_url_weight += rarity_weight;
            }
        }

        // Base scores are guaranteed to be between 0.0 and 1.0
        double title_base_score = (total_rarity_weight > 0.0) ? (matched_title_weight / total_rarity_weight) : 0.0;
        double url_base_score   = (total_rarity_weight > 0.0) ? (matched_url_weight / total_rarity_weight) : 0.0;

        // FACTOR 3: Title Rarity Match + Length Penalty (Range: 0.0 - 1.0)
        double factor_3 = title_base_score * exp(-Gamma_title * r.title.size());

        // FACTOR 4: Anchor Text Diversity + Bounded Volume (Range: 0.0 - 1.0)
        double factor_4 = 0.0;
        if (r.total_link_frequency_anchor > 0 && r.unique_phrases_matched_anchor > 0) {
            
            // 1. Bounded Volume: log(1+F) / (log(1+F) + 2.0)
            // Even if F = 1,000,000, this curve safely asymptotes just under 1.0
            double log_f = log(1.0 + (double)r.total_link_frequency_anchor);
            double volume_score = log_f / (log_f + 2.0); 

            // 2. Diversity Multiplier: U / (U + K)
            const double K = 2.0; // Strictness tuning constant
            double diversity_multiplier = (double)r.unique_phrases_matched_anchor / ((double)r.unique_phrases_matched_anchor + K);

            factor_4 = volume_score * diversity_multiplier;
        };

        // FACTOR 5: URL Rarity Match + Length Penalty (Range: 0.0 - 1.0)
        double factor_5 = url_base_score * exp(-Gamma_url * r.url.size());

        // This factor scores based on the rarity of each word combined with its frequency
        vector<int> counts(r.word_positions.size());
        for(size_t i = 0; i < r.word_positions.size(); i++) {
            counts[i] = r.word_positions[i].size();
        }
        double factor_6 = word_rarity_freq_score(r);

        double factor_7 = phrase_bonus(r.word_positions);

        double factor_8 = early_position_score(
            r.word_positions,
            r.doc_len,
            unique_query_terms
        );

        // FACTOR 9: Domain Match (rarity-weighted, case-insensitive).
        // Extract the host (between "://" and the first '/', '?', '#', or ':'),
        // then count which query terms appear as substrings within it.
        // Gives a strong, dedicated signal that rewards "wikipedia" → wikipedia.org
        // over "wikipedia" appearing anywhere in path/body.
        size_t dom_start = 0;
        {
            const char* d = r.url.data();
            size_t sz = r.url.size();
            size_t i = 0;
            while (i + 2 < sz && d[i] != ':') i++;
            if (i + 2 < sz && d[i] == ':' && d[i+1] == '/' && d[i+2] == '/') {
                dom_start = i + 3;
            }
        }
        size_t dom_end = dom_start;
        {
            const char* d = r.url.data();
            size_t sz = r.url.size();
            while (dom_end < sz) {
                char c = d[dom_end];
                if (c == '/' || c == '?' || c == '#' || c == ':') break;
                dom_end++;
            }
        }
        double matched_domain_weight = 0.0;
        if (dom_end > dom_start) {
            const char* host = r.url.data() + dom_start;
            size_t host_len = dom_end - dom_start;
            for (size_t i = 0; i < unique_query_terms.size(); i++) {
                const string& term = unique_query_terms[i].phrase;
                if (term.size() == 0 || term.size() > host_len) continue;
                double rarity_weight = 1.0 / (1.0 + log(1.0 + (double)unique_query_terms[i].freq));
                bool found = false;
                const char* nd = term.data();
                size_t nd_len = term.size();
                for (size_t j = 0; j + nd_len <= host_len && !found; j++) {
                    bool ok = true;
                    for (size_t k = 0; k < nd_len; k++) {
                        char hc = host[j+k];
                        if (hc >= 'A' && hc <= 'Z') hc += 32;
                        char tc = nd[k];
                        if (tc >= 'A' && tc <= 'Z') tc += 32;
                        if (hc != tc) { ok = false; break; }
                    }
                    if (ok) found = true;
                }
                if (found) matched_domain_weight += rarity_weight;
            }
        }
        double factor_9 = (total_rarity_weight > 0.0) ? (matched_domain_weight / total_rarity_weight) : 0.0;

        // FACTOR 10: Canonical Article Match (domain-gated).
        // Slug = last path segment (between final '/' and first '?'/'#'/end).
        // A query term "matches" the slug if it is the entire slug
        // (case-insensitive), or a prefix followed by a URL word boundary
        // ('-', '_', '.'). ':' is deliberately NOT a boundary so Wikipedia
        // namespace pages ("/wiki/Wikipedia:WikiProject_Foo") don't match
        // on "wikipedia". Multiplied by factor_9 so the bonus only fires
        // on pages whose host already matches the query — spam sites
        // can't earn it by stuffing the query into their path.
        double slug_match_score = 0.0;
        {
            const char* d = r.url.data();
            size_t sz = r.url.size();
            size_t path_end = dom_end;
            while (path_end < sz && d[path_end] != '?' && d[path_end] != '#') path_end++;
            size_t slug_start = dom_end;
            for (size_t i = dom_end; i < path_end; i++) {
                if (d[i] == '/') slug_start = i + 1;
            }
            size_t slug_len = path_end - slug_start;
            if (slug_len > 0) {
                const char* slug = d + slug_start;
                double matched_slug_weight = 0.0;
                for (size_t i = 0; i < unique_query_terms.size(); i++) {
                    const string& term = unique_query_terms[i].phrase;
                    size_t tsz = term.size();
                    if (tsz == 0 || tsz > slug_len) continue;
                    const char* td = term.data();
                    bool prefix = true;
                    for (size_t k = 0; k < tsz; k++) {
                        char sc = slug[k]; if (sc >= 'A' && sc <= 'Z') sc += 32;
                        char tc = td[k];   if (tc >= 'A' && tc <= 'Z') tc += 32;
                        if (sc != tc) { prefix = false; break; }
                    }
                    if (!prefix) continue;
                    if (tsz != slug_len) {
                        char next = slug[tsz];
                        if (next != '-' && next != '_' && next != '.') continue;
                    }
                    double rarity_weight = 1.0 / (1.0 + log(1.0 + (double)unique_query_terms[i].freq));
                    matched_slug_weight += rarity_weight;
                }
                slug_match_score = (total_rarity_weight > 0.0) ? (matched_slug_weight / total_rarity_weight) : 0.0;
            }
        }
        double factor_10 = slug_match_score * factor_9;

        double final_score = ((factor_1 * factor_1_weight) +
            (factor_2 * factor_2_weight) +
            (factor_3 * factor_3_weight) +
            (factor_4 * factor_4_weight) +
            (factor_5 * factor_5_weight) +
            (factor_6 * factor_6_weight) +
            (factor_7 * factor_7_weight) +
            (factor_8 * factor_8_weight) +
            (factor_9 * factor_9_weight) +
            (factor_10 * factor_10_weight))
            / dynamic_weight_sum;

        if (verbose_mode) {
            // printf("Unique words found URL: %d", r.num_unique_words_found_url);
            printf("\n--- DYNAMIC SCORING [%.*s] ---\n", (int)r.url.size(), r.url.data());
            printf("F1 (Proximity)  : %.6f * %.2f = %.6f\n", factor_1, factor_1_weight, factor_1 * factor_1_weight);
            printf("F2 (Times Seen) : %.6f * %.2f = %.6f\n", factor_2, factor_2_weight, factor_2 * factor_2_weight);
            printf("F3 (Title Match): %.6f * %.2f = %.6f\n", factor_3, factor_3_weight, factor_3 * factor_3_weight);
            printf("F4 (Anchor Txt) : %.6f * %.2f = %.6f\n", factor_4, factor_4_weight, factor_4 * factor_4_weight);
            printf("F5 (URL Match)  : %.6f * %.2f = %.6f\n", factor_5, factor_5_weight, factor_5 * factor_5_weight);
            printf("F6 (Frequency)  : %.6f * %.2f = %.6f\n", factor_6, factor_6_weight, factor_6 * factor_6_weight);
            printf("F7 (Exact Phrase)  : %.6f * %.2f = %.6f\n", factor_7, factor_7_weight, factor_7 * factor_7_weight);
            printf("F8 (Early Position): %.6f * %.2f = %.6f\n", factor_8, factor_8_weight, factor_8 * factor_8_weight);
            printf("F9 (Domain Match)  : %.6f * %.2f = %.6f\n", factor_9, factor_9_weight, factor_9 * factor_9_weight);
            printf("F10 (Canon Slug)   : %.6f * %.2f = %.6f\n", factor_10, factor_10_weight, factor_10 * factor_10_weight);
            printf("FINAL DYNAMIC SCORE: %.6f\n", final_score);
            printf("--------------------------------------\n");
            fflush(stdout); // Ensures it prints
        }

        return final_score;
    }
    
    void set_new_query(DocInfo &dih) {
        if(unique_query_terms.size() != 0) {
            logger::debug("Ranker already has had a query set");
            return;
        } 

        for (NodeInfo& n : dih.nodeInfo) {
            if(n.freq < rarestTerm.freq) {
                rarestTerm = QueryInfo{string(n.phrase.data(), n.phrase.size()), n.freq, n.is_phrase};
            }
            unique_query_terms.push_back(QueryInfo{string(n.phrase.data(), n.phrase.size()), n.freq, n.is_phrase});
        }
        
        if(verbose_mode) {
            logger::debug("Ranker has been changed to serving an amount of %zu unique words in the query", unique_query_terms.size());
        }
        
        // Reset PQ using SmallPQ
        pq = SmallPQ(num_pages_returned);
    }

    vector<LeanPage> rank(vector<RankedPage> &v) {
        // Initialize the pq
        pq = SmallPQ(num_pages_returned);

        for(size_t i = 0; i < v.size(); i++) {
            // calculate the score of the page
            double r_score = calc_static_score(v[i]) * (1-dynamic_weight) + calc_dynamic_score(v[i]) * dynamic_weight;

            if(verbose_mode) {
                logger::debug("The URL %.*s earned a score of: %.6f", (int)v[i].url.size(), v[i].url.data(), r_score);
            }

            if (url_host_contains_ci(v[i].url, "wikipedia") || url_host_contains_ci(v[i].url, "anduril")) {
                logger::instr("[RANKER] tracked url scored %.6f: %.*s",
                              r_score, (int)v[i].url.size(), v[i].url.data());
            }

            // Push directly to SmallPQ. It handles bounded limits and sorting automatically!
            pq.push(LeanPage{std::move(v[i].url), std::move(v[i].title), r_score});
        }
        
        if(verbose_mode) {
            logger::debug("Ranker has finished ranking elements");
        }

        // Return the internally managed, perfectly sorted vector from SmallPQ
        return pq.getResults();
    }

public:
    // Added pq to the initializer list to ensure it's sized correctly on creation
    Ranker(UrlStore* url_store, int num_pages_returned_init = RANKED_ON_EACH, double dynamic_weight_init = DEFAULT_DYNAMIC_WEIGHT, bool verbose_init = false) : 
        pq(num_pages_returned_init),
        dynamic_weight(dynamic_weight_init), num_pages_returned(num_pages_returned_init), verbose_mode(verbose_init), is_query_set(false), url_store(url_store) { 
        
        if(verbose_mode) {
            logger::debug("Ranker is initialized with num pages returned of %d, and dynamic weighting of %f", 
                num_pages_returned_init, dynamic_weight_init);
        }
    }

    vector<LeanPage> processQueryResponse(ChunkQueryInfo& cqi) {
        vector<LeanPage> result;
        vector<RankedPage> candidates;
        vector<RankerNodeInfo> ranker_info;
        if(!is_query_set && cqi.pages.size() > 0) {
            is_query_set = true;
            set_new_query(cqi.pages[0]);
        }
        for (DocInfo& di : cqi.pages) {
            const string& url = di.url;
            const vector<NodeInfo>& phrases = di.nodeInfo;
            RankedPage page;
            page.url = string(url.data(), url.size());
            auto data = url_store->getUrl(url);
            s_total_count.fetch_add(1, std::memory_order_relaxed);
            if (!data) {
                s_miss_count.fetch_add(1, std::memory_order_relaxed);
                if (url_host_contains_ci(url, "wikipedia") || url_host_contains_ci(url, "anduril")) {
                    logger::instr("[RANKER] urlstore MISS for tracked url: %.*s",
                                  (int)url.size(), url.data());
                }
            }
            if (data) {
                page.title = string(data->title.data(), data->title.size());
                page.seed_list_dist = data->seed_distance;
                page.domains_from_seed = data->domain_dist;
                // page.num_unique_words_found_anchor = data->anchor_freqs.size();

                // 1. Get the resolved anchor data for the URL
                vector<UrlAnchorData> anchors = url_store->getUrlAnchorInfo(url);

                page.unique_phrases_matched_anchor = 0;
                page.total_link_frequency_anchor = 0;

                // 2. Loop through and check the actual text
                for (const auto& anchor : anchors) {
                    const string& text = *(anchor.anchor_text);
                    
                    // Check if the target phrase exists in this anchor text
                    if (text.contains(rarestTerm.phrase)) {
                        page.unique_phrases_matched_anchor++;           // Count how many unique phrases matched
                        page.total_link_frequency_anchor += anchor.freq; // Count the total number of incoming links
                    }
                }
                
                for (const NodeInfo& ni : phrases) {
                    const string& phrase = ni.phrase;
                    // printf("%s", phrase.data());
                    page.title_words_found.push_back(data->title.contains(phrase));
                    page.url_words_found.push_back(url.contains(phrase));
                    // page.num_unique_words_found_title += data->title.contains(phrase);
                    // page.num_unique_words_found_url += url.contains(phrase);
                }
                
                page.doc_len = data->eod;
                page.times_seen = data->num_encountered; 

                vector<vector<size_t>> word_positions_init;
                for(NodeInfo &n : di.nodeInfo) {
                    word_positions_init.push_back(n.pos);
                }
                page.word_positions = word_positions_init;

                candidates.push_back(std::move(page));;
            }
        }
        return rank(candidates);
    }

    double testScore(RankedPage &page, vector<QueryInfo> terms) {
        if(!is_query_set) {
            is_query_set = true;
            for(QueryInfo &q : terms) {
                unique_query_terms.push_back(std::move(q));
            }
            // Reset PQ
            pq = SmallPQ(num_pages_returned);
        }
        return (calc_static_score(page, verbose_mode) * (1-dynamic_weight) + 
            calc_dynamic_score(page) * dynamic_weight);
    }

    double getScore(DocInfo &dih) {
        const string& url = dih.url;
        const vector<NodeInfo>& phrases = dih.nodeInfo;
        RankedPage page;
        if(!is_query_set) {
            is_query_set = true;
            set_new_query(dih);
        }
        page.url = string(url.data(), url.size());
        auto data = url_store->getUrl(url);
        if (data) {
            page.title = string(data->title.data(), data->title.size());
            page.seed_list_dist = data->seed_distance;
            page.domains_from_seed = data->domain_dist;
            // page.num_unique_words_found_anchor = data->anchor_freqs.size();

            // 1. Get the resolved anchor data for the URL
            vector<UrlAnchorData> anchors = url_store->getUrlAnchorInfo(url);

            page.unique_phrases_matched_anchor = 0;
            page.total_link_frequency_anchor = 0;

            // 2. Loop through and check the actual text
            for (const auto& anchor : anchors) {
                const string& text = *(anchor.anchor_text);
                
                // Check if the target phrase exists in this anchor text
                if (text.contains(rarestTerm.phrase)) {
                    page.unique_phrases_matched_anchor++;           // Count how many unique phrases matched
                    page.total_link_frequency_anchor += anchor.freq; // Count the total number of incoming links
                }
            }

            for (const NodeInfo& ni : phrases) {
                    const string& phrase = ni.phrase;
                    // printf("%s", phrase.data());
                    page.title_words_found.push_back(data->title.contains(phrase));
                    page.url_words_found.push_back(url.contains(phrase));
                    // page.num_unique_words_found_title += data->title.contains(phrase);
                    // page.num_unique_words_found_url += url.contains(phrase);
            }
            
            page.doc_len = data->eod;
            page.times_seen = data->num_encountered; 

            vector<vector<size_t>> word_positions_init;
            for(NodeInfo &n : dih.nodeInfo) {
                word_positions_init.push_back(n.pos);
            }
            page.word_positions = word_positions_init;

            double r_score = calc_static_score(page) * (1-dynamic_weight) + 
                calc_dynamic_score(page) * dynamic_weight;
            return r_score;
        } else {
            return -1.0;
        }
    }
};