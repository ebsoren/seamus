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
    int seed_list_dist = 0;
    int domains_from_seed = 0;
    int num_unique_words_found_anchor = 0;
    int num_unique_words_found_title = 0;
    int num_unique_words_found_url = 0;
    int times_seen = 0;
    vector<vector<size_t>> word_positions;
    size_t doc_len = 0;
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
inline double calc_static_score(const RankedPage &p) {
    
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
    double factor_3 = max(double_pow(e, factor_3_const * p.seed_list_dist), 0.1);  // NOTE: may need to tune the constant here

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
        (factor_9 * static_9_weight)) 
        / static_weight_sum);
}

template <typename T>
T min_element(vector<T> &v) {
    if (v.size() == 0) return T{};
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
    if (v.size() == 0) return T{};
    T max_elt = v[0];
    for(size_t i = 1; i < v.size(); i++) {
        if(v[i] > max_elt) {
            max_elt = v[i];
        }
    }
    return max_elt;
}


inline double word_pos_score(const vector<vector<size_t>> &positions, size_t doc_len, int unique_words_in_query) {
    if (unique_words_in_query == 0 || doc_len == 0) return 0.0;

    double score = 0.0;

    for (int i = 0; i < unique_words_in_query; i++) {
        // Using log() heavily rewards the first few mentions of a word, 
        score += log(1.0 + positions[i].size()); 
    }

    // O(N + M) TWO-POINTER PROXIMITY SCORE
    double proximity_score = 0.0;

    for (int i = 0; i < unique_words_in_query; i++) {
        for (int j = i + 1; j < unique_words_in_query; j++) {

            const auto& posA = positions[i];
            const auto& posB = positions[j];
            
            if (posA.size() == 0 || posB.size() == 0) continue;
            
            size_t b = 0; // ptr
            
            for (size_t a = 0; a < posA.size(); ++a) {
                size_t p = posA[a];
                
                while (b + 1 < posB.size()) {
                    size_t current_dist = (p > posB[b]) ? (p - posB[b]) : (posB[b] - p);
                    size_t next_dist = (p > posB[b+1]) ? (p - posB[b+1]) : (posB[b+1] - p);
                    
                    if (next_dist <= current_dist) {
                        b++; 
                    } else {
                        break; 
                    }
                }
                
                size_t best_dist = (p > posB[b]) ? (p - posB[b]) : (posB[b] - p);
                    proximity_score += 1.0 / (1.0 + best_dist);
            }
        }
    }

    score += LAMBDA_POS * proximity_score;

    // normalizing using log and doc length
    double normalized = score / (1.0 + log(1.0 + doc_len));

    // Ensuring final factor is between 0.1-1.0
    if (normalized > 1.0) normalized = 1.0;
    if (normalized < 0.1) normalized = 0.1;

    return normalized;
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
};

class Ranker {
private:
    priority_queue<LeanPage, vector<LeanPage>, RankedCompare> pq;
    double dynamic_weight;
    vector<QueryInfo> unique_query_terms; 
    int num_pages_returned;
    bool verbose_mode;
    bool is_query_set;
    UrlStore* url_store;

    double word_rarity_freq_score(RankedPage &r) {

        int n = unique_query_terms.size();
        vector<double> x(n);
        
        // double sum_freq = 0.0;
        // for (int i = 0; i < n; i++) {
        //     sum_freq += (double)unique_query_terms[i].freq;
        // }

        // log rarity
        for (int i = 0; i < n; i++) {
            x[i] = -log(1.0 + (double)unique_query_terms[i].freq);
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
        double raw = 0.0, max_possible = 0.0;

        for (int i = 0; i < n; i++) {
            double f = 1 - exp(-LAMBDA_FREQ * (double)r.word_positions[i].size());
            raw += w[i] * f;
            max_possible += w[i];
        }

        double S = (max_possible > 0) ? raw / max_possible : 0.0;

        // scale to [0.2, 1]
        double final_score = 0.1 + 0.9 * S;
        
        return final_score;
    }

    double calc_dynamic_score(RankedPage &r) {
        // This factor checks frequency of unique words and proximity scores of the unique words to other unique words in the query 
        double factor_1 = word_pos_score(r.word_positions, r.doc_len, unique_query_terms.size());

        // This factor scores based on number of times the link was seen during crawling
        double factor_2 = max(1.0 / (1.0 + double_pow(e, -k * (r.times_seen - n_0))), 0.2);

        // This factor scores based on how many unique words in the query were found in the title but penalized on length of the title
        double factor_3 = ((double)r.num_unique_words_found_title / unique_query_terms.size()) * double_pow(e, (-Gamma_title * r.title.size()));

        // This factor checks unique words in the query found in the anchor texts pointing to the link
        // PROBABLY WANT TO ADD MORE TO THIS FACTOR ONCE I KNOW MORE ABOUT ANCHOR TEXT
        double factor_4 = ((double)r.num_unique_words_found_anchor / unique_query_terms.size()); 

        // This factor checks the URL for keywords in it
        double factor_5 = ((double)r.num_unique_words_found_url / unique_query_terms.size()) * double_pow(e, (-Gamma_url * r.url.size()));

        // This factor scores based on the rarity of each word combined with its frequency
        vector<int> counts(r.word_positions.size());
        for(size_t i = 0; i < r.word_positions.size(); i++) {
            counts[i] = r.word_positions[i].size();
        }
        double factor_6 = word_rarity_freq_score(r);

        // final score returned here with extra weightings 
        return(((factor_1 * factor_1_weight) + 
            (factor_2 * factor_2_weight) + 
            (factor_3 * factor_3_weight) + 
            (factor_4 * factor_4_weight) + 
            (factor_5 * factor_5_weight) + 
            (factor_6 * factor_6_weight)) 
            / dynamic_weight_sum);
    }
    
    void set_new_query(DocInfo &dih) {
        // TODO(erik): retrieve all the unique nodes from the whole query
        if(unique_query_terms.size() != 0) {
            logger::debug("Ranker already has had a query set");
            return;
        } 
        for (NodeInfo& n : dih.nodeInfo) {
            unique_query_terms.push_back(QueryInfo{string(n.phrase.data(), n.phrase.size()), n.freq, n.is_phrase});
        }
        
        if(verbose_mode) {
            logger::debug("Ranker has been changed to serving an amount of %zu unique words in the query", unique_query_terms.size());
        }
        pq = priority_queue<LeanPage, vector<LeanPage>, RankedCompare>(
            RankedCompare(dynamic_weight, unique_query_terms.size()),
            vector<LeanPage>()
        );
    }

        vector<LeanPage> rank(vector<RankedPage> &v) {
        //initialize the pq
        pq = priority_queue<LeanPage, vector<LeanPage>, RankedCompare>(
            RankedCompare(dynamic_weight, unique_query_terms.size()),
            vector<LeanPage>()
        );

        vector<LeanPage> input;
        for(size_t i = 0; i < v.size(); i++) {
            // calculate the score of the page
            double r_score = calc_static_score(v[i]) * (1-dynamic_weight) + calc_dynamic_score(v[i]) * dynamic_weight;

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
        for(int i = 0; i < min(num_pages_returned, pq.size()); i++) {
            results.push_back(pq.pop_move());
        }
        if(verbose_mode) {
            logger::debug("Ranker is returning vector of %zu elements", results.size());
        }

        // reverse results so that the highest ranked is first in the ordering
        reverse(results);

        return(results);
    }

public:
    Ranker(UrlStore* url_store, int num_pages_returned_init = RANKED_ON_EACH, double dynamic_weight_init = DEFAULT_DYNAMIC_WEIGHT, bool verbose_init = false) : 
        dynamic_weight(dynamic_weight_init), num_pages_returned(num_pages_returned_init), verbose_mode(verbose_init), is_query_set(false), url_store(url_store) { 
        
        if(verbose_mode) {
            logger::debug("Ranker is initialized with num pages returned of %d, and dynamic weighting of %f", 
                num_pages_returned_init, dynamic_weight_init);
        }
    }

    vector<LeanPage> processQueryResponse(ChunkQueryInfo& cqi) {
        vector<LeanPage> result;
        vector<RankedPage> candidates;
        vector<RankerNodeInfo> ranker_info; // TODO: construct this for ranker.set_new_query()
        if(!is_query_set && cqi.pages.size() > 0) {
            is_query_set = true;
            set_new_query(cqi.pages[0]);
        }
        size_t miss_count = 0;
        for (DocInfo& di : cqi.pages) {
            const string& url = di.url;
            const vector<NodeInfo>& phrases = di.nodeInfo;
            RankedPage page;
            page.url = string(url.data(), url.size());
            auto data = url_store->getUrl(url);
            if (!data) {
                if (miss_count < 3) {
                    fprintf(stderr, "[RANKER] url NOT in urlstore (len=%zu): '%.*s'\n",
                            url.size(), static_cast<int>(url.size()), url.data());
                }
                miss_count++;
            }
            if (data) {
                page.title = string(data->title.data(), data->title.size());
                page.seed_list_dist = data->seed_distance;
                page.domains_from_seed = data->domain_dist;
                page.num_unique_words_found_anchor = data->anchor_freqs.size();

                // Work to calculate:
                // int num_unique_words_found_title;
                // int num_unique_words_found_url;
                for (const NodeInfo& ni : phrases) {
                    const string& phrase = ni.phrase;
                    page.num_unique_words_found_title += data->title.contains(phrase);
                    page.num_unique_words_found_url += url.contains(phrase);
                }
                
                page.doc_len = data->eod;
                page.times_seen = data->num_encountered; 

                // populate word_positions
                vector<vector<size_t>> word_positions_init;
                for(NodeInfo &n : di.nodeInfo) {
                    word_positions_init.push_back(n.pos);
                }
                page.word_positions = word_positions_init;

                candidates.push_back(std::move(page));;
            } else {
                logger::error("from ranker -- url not found: %s", url.data());
            }
        }
        return rank(candidates);
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
            page.num_unique_words_found_anchor = data->anchor_freqs.size();

            // Work to calculate:
            // int num_unique_words_found_title;
            // int num_unique_words_found_url;
            for (const NodeInfo& ni : phrases) {
                const string& phrase = ni.phrase;
                page.num_unique_words_found_title += data->title.contains(phrase);
                page.num_unique_words_found_url += url.contains(phrase);
            }
            
            page.doc_len = data->eod;
            page.times_seen = data->num_encountered; 

            // TODO: populate word_positions
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

