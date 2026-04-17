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
        T temp = std::move(vec[i]);
        vec[i] = std::move(vec[n - 1 - i]);
        vec[n - 1 - i] = std::move(temp);
    }
}

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
    string node;
    size_t cnt;
    bool is_phrase;
};

inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}


// TLD quality weights
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
inline unordered_map<string, double> tldWeight = makeTldWeight();

inline double max(double i, double j) { return i < j ? j : i; }
inline double min(double i, double j) { return i < j ? i : j; }

template <typename T>
T min_element(vector<T> &v) {
    if (v.size() == 0) return T{};
    T m = v[0];
    for (size_t i = 1; i < v.size(); i++) if (v[i] < m) m = v[i];
    return m;
}
template <typename T>
T max_element(vector<T> &v) {
    if (v.size() == 0) return T{};
    T m = v[0];
    for (size_t i = 1; i < v.size(); i++) if (v[i] > m) m = v[i];
    return m;
}

// ---------------------------------------------------------------------------
// Static score: URL quality signals independent of the query
// ---------------------------------------------------------------------------
inline double calc_static_score(const RankedPage &p) {

    string_view url = p.url.str_view(0, p.url.size());
    size_t start_pos = 8; // skip "https://"
    if (url.size() >= 7 && url[4] != 's') start_pos = 7; // "http://"

    int subdomain_count = 0;
    int digit_count_domain = 0;
    double domain_size = 0.0;
    int path_depth = 0;
    bool qmarkfound = false;
    size_t ext_start = 0;
    size_t ext_len = 0;

    for (size_t i = start_pos; i < url.size(); i++) {
        char c = url[i];
        if (c == '/' || c == '?' || c == '#' || c == ':') {
            for (; i < url.size(); i++) {
                if (url[i] == '/') path_depth++;
                else if (url[i] == '?') qmarkfound = true;
            }
            break;
        } else if (c == '.') {
            subdomain_count++;
            ext_start = i + 1;
            ext_len = 0;
        } else {
            ext_len++;
            if (is_digit(c)) digit_count_domain++;
        }
        domain_size += 1.0;
    }

    string_view extension = url.substr(ext_start, ext_len);
    auto slot = tldWeight.find(extension);
    double tld_score = (slot == tldWeight.end()) ? 0.2 : (*slot).value;

    // Domain distance from seed (crawl graph depth)
    double domain_dist_score = max(double_pow(e, -0.08 * p.domains_from_seed), 0.2);

    // Seed list distance
    double seed_score = max(double_pow(e, factor_3_const * p.seed_list_dist), 0.1);

    // Domain length penalty
    double domain_len_score = max((50.0 - domain_size) / 50.0, 0.2);

    // Subdomain penalty
    double subdomain_score = 1.0 / (1.0 + 0.1 * subdomain_count);

    // Digits in domain penalty
    double digit_score = 1.0 / (1.0 + 0.15 * digit_count_domain);

    // URL length penalty
    double url_len_score = max((150.0 - (double)url.size()) / 100.0, 0.2);

    // Path depth penalty
    double path_score = max(1.0 - 0.1 * path_depth, 0.2);

    // Query string penalty (reduced, not eliminated)
    double query_score = qmarkfound ? 0.4 : 1.0;

    return ((domain_dist_score * static_1_weight) +
            (tld_score * static_2_weight) +
            (seed_score * static_3_weight) +
            (domain_len_score * static_4_weight) +
            (subdomain_score * static_5_weight) +
            (digit_score * static_6_weight) +
            (url_len_score * static_7_weight) +
            (path_score * static_8_weight) +
            (query_score * static_9_weight))
           / static_weight_sum;
}


// ---------------------------------------------------------------------------
// BM25-inspired term frequency score
// ---------------------------------------------------------------------------
inline double bm25_score(const vector<vector<size_t>>& positions, size_t doc_len,
                         const vector<uint64_t>& term_freqs, size_t total_docs) {
    if (doc_len == 0 || positions.size() == 0) return 0.0;

    constexpr double k1 = 1.2;
    constexpr double b = 0.75;
    constexpr double avg_dl = 500.0; // rough average doc length estimate

    double score = 0.0;
    for (size_t i = 0; i < positions.size(); i++) {
        double tf = (double)positions[i].size();
        if (tf == 0.0) continue;

        // IDF: log((N - df + 0.5) / (df + 0.5) + 1)
        double df = (double)term_freqs[i];
        if (df < 1.0) df = 1.0;
        double N = (double)total_docs;
        if (N < df) N = df;
        double idf = log((N - df + 0.5) / (df + 0.5) + 1.0);

        // BM25 TF saturation
        double dl = (double)doc_len;
        double tf_norm = (tf * (k1 + 1.0)) / (tf + k1 * (1.0 - b + b * (dl / avg_dl)));

        score += idf * tf_norm;
    }
    return score;
}


// ---------------------------------------------------------------------------
// Proximity score: reward query terms appearing near each other
// ---------------------------------------------------------------------------
inline double proximity_score(const vector<vector<size_t>>& positions, int num_terms) {
    if (num_terms < 2) return 0.0;

    double score = 0.0;
    for (int i = 0; i < num_terms; i++) {
        for (int j = i + 1; j < num_terms; j++) {
            const auto& posA = positions[i];
            const auto& posB = positions[j];
            if (posA.size() == 0 || posB.size() == 0) continue;

            size_t b = 0;
            for (size_t a = 0; a < posA.size(); ++a) {
                size_t p = posA[a];
                while (b + 1 < posB.size()) {
                    size_t cur = (p > posB[b]) ? (p - posB[b]) : (posB[b] - p);
                    size_t nxt = (p > posB[b+1]) ? (p - posB[b+1]) : (posB[b+1] - p);
                    if (nxt <= cur) b++;
                    else break;
                }
                size_t dist = (p > posB[b]) ? (p - posB[b]) : (posB[b] - p);
                score += 1.0 / (1.0 + dist);
            }
        }
    }
    return score;
}


struct QueryTerm {
    string phrase;
    uint64_t freq; // global corpus frequency (n_posts or n_docs)
    bool is_phrase;
};

struct RankedCompare {
    bool operator()(const LeanPage& a, const LeanPage& b) const {
        return a.score > b.score;
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
        logger::warn("[RANKER] GLOBAL: %zu/%zu docs missed urlstore (%.1f%%)",
                misses, total, pct);
    }

private:
    priority_queue<LeanPage, vector<LeanPage>, RankedCompare> pq;
    double dynamic_weight;
    vector<QueryTerm> query_terms;
    int num_pages_returned;
    UrlStore* url_store;
    bool query_set = false;

    void init_query(DocInfo& di) {
        if (query_set) return;
        query_set = true;
        for (NodeInfo& n : di.nodeInfo) {
            query_terms.push_back(QueryTerm{
                string(n.phrase.data(), n.phrase.size()),
                n.freq,
                n.is_phrase
            });
        }
    }

    // Count how many query terms appear in the anchor texts pointing to this URL
    int count_anchor_term_matches(UrlData* data) {
        if (!data || data->anchor_freqs.size() == 0) return 0;

        // Collect anchor texts for this URL
        vector<string*> anchor_texts;
        {
            std::lock_guard<std::mutex> lock_global(url_store->global_mtx);
            for (auto it = data->anchor_freqs.begin(); it != data->anchor_freqs.end(); ++it) {
                uint32_t anchor_id = (*it).key;
                if (anchor_id < url_store->id_to_anchor.size()) {
                    anchor_texts.push_back(&url_store->id_to_anchor[anchor_id]);
                }
            }
        }

        int matches = 0;
        for (size_t i = 0; i < query_terms.size(); i++) {
            for (size_t j = 0; j < anchor_texts.size(); j++) {
                if (anchor_texts[j]->contains(query_terms[i].phrase)) {
                    matches++;
                    break;
                }
            }
        }
        return matches;
    }

    double calc_dynamic_score(RankedPage& r) {
        int n = query_terms.size();
        if (n == 0) return 0.0;

        // Estimate doc_len from positions if eod wasn't set
        size_t effective_doc_len = r.doc_len;
        if (effective_doc_len == 0) {
            for (size_t i = 0; i < r.word_positions.size(); i++) {
                for (size_t j = 0; j < r.word_positions[i].size(); j++) {
                    if (r.word_positions[i][j] > effective_doc_len)
                        effective_doc_len = r.word_positions[i][j];
                }
            }
            effective_doc_len += 1;
        }

        // Build term freq vector for BM25
        vector<uint64_t> term_freqs;
        term_freqs.reserve(n);
        for (int i = 0; i < n; i++) {
            term_freqs.push_back(query_terms[i].freq > 0 ? query_terms[i].freq : 1);
        }

        // Factor 1: BM25 score (term frequency + IDF)
        // Normalize to roughly [0, 1] with a sigmoid
        double raw_bm25 = bm25_score(r.word_positions, effective_doc_len, term_freqs, 100000);
        double factor_1 = 1.0 - 1.0 / (1.0 + 0.1 * raw_bm25);

        // Factor 2: Popularity (times seen during crawl)
        double factor_2 = max(1.0 / (1.0 + double_pow(e, -k * (r.times_seen - n_0))), 0.2);

        // Factor 3: Query words in title
        double title_match_ratio = (double)r.num_unique_words_found_title / n;
        double factor_3 = title_match_ratio;

        // Factor 4: Query words in anchor texts
        double factor_4 = (double)r.num_unique_words_found_anchor / n;

        // Factor 5: Query words in URL
        double factor_5 = (double)r.num_unique_words_found_url / n;

        // Factor 6: Proximity of query terms in document
        double prox = proximity_score(r.word_positions, n);
        double factor_6 = 1.0 - 1.0 / (1.0 + LAMBDA_POS * prox);

        return ((factor_1 * factor_1_weight) +
                (factor_2 * factor_2_weight) +
                (factor_3 * factor_3_weight) +
                (factor_4 * factor_4_weight) +
                (factor_5 * factor_5_weight) +
                (factor_6 * factor_6_weight))
               / dynamic_weight_sum;
    }

    vector<LeanPage> rank(vector<RankedPage>& v) {
        pq = priority_queue<LeanPage, vector<LeanPage>, RankedCompare>(
            RankedCompare{}, vector<LeanPage>()
        );

        for (size_t i = 0; i < v.size(); i++) {
            double r_score = calc_static_score(v[i]) * (1.0 - dynamic_weight) +
                             calc_dynamic_score(v[i]) * dynamic_weight;

            if ((int)pq.size() < num_pages_returned) {
                pq.push(LeanPage{std::move(v[i].url), std::move(v[i].title), r_score});
            } else if (r_score > pq.front().score) {
                pq.push(LeanPage{std::move(v[i].url), std::move(v[i].title), r_score});
                pq.pop();
            }
        }

        vector<LeanPage> results;
        int count = pq.size() < (size_t)num_pages_returned ? pq.size() : num_pages_returned;
        for (int i = 0; i < count; i++) {
            results.push_back(pq.pop_move());
        }
        reverse(results);
        return results;
    }

public:
    Ranker(UrlStore* url_store, int num_pages_returned_init = RANKED_ON_EACH,
           double dynamic_weight_init = DEFAULT_DYNAMIC_WEIGHT, bool verbose_init = false)
        : dynamic_weight(dynamic_weight_init), num_pages_returned(num_pages_returned_init),
          url_store(url_store) {
        (void)verbose_init;
    }

    vector<LeanPage> processQueryResponse(ChunkQueryInfo& cqi) {
        vector<RankedPage> candidates;

        if (cqi.pages.size() == 0) return {};
        init_query(cqi.pages[0]);

        for (DocInfo& di : cqi.pages) {
            const string& url = di.url;
            const vector<NodeInfo>& phrases = di.nodeInfo;

            RankedPage page;
            page.url = string(url.data(), url.size());

            // Populate word positions from index data
            page.word_positions.reserve(di.nodeInfo.size());
            for (NodeInfo& n : di.nodeInfo) {
                page.word_positions.push_back(std::move(n.pos));
            }

            UrlData* data = url_store ? url_store->getUrl(url) : nullptr;
            s_total_count.fetch_add(1, std::memory_order_relaxed);
            if (!data) {
                logger::warn("[RANKER] url NOT in urlstore (len=%zu): '%.*s'",
                        url.size(), static_cast<int>(url.size()), url.data());
                s_miss_count.fetch_add(1, std::memory_order_relaxed);
            }
            if (data) {
                logger::warn("[RANKER] url IN urlstore (len=%zu): '%.*s'",
                        url.size(), static_cast<int>(url.size()), url.data());
                page.title = string(data->title.data(), data->title.size());
                page.seed_list_dist = data->seed_distance;
                page.domains_from_seed = data->domain_dist;
                page.times_seen = data->num_encountered;
                page.doc_len = data->eod;

                // Count query terms found in title and URL
                for (const NodeInfo& ni : phrases) {
                    if (data->title.size() > 0 && data->title.contains(ni.phrase))
                        page.num_unique_words_found_title++;
                    if (url.contains(ni.phrase))
                        page.num_unique_words_found_url++;
                }

                // Count query terms found in anchor texts
                page.num_unique_words_found_anchor = count_anchor_term_matches(data);
            } else {
                // No urlstore data — still rank with position-based signals
                page.seed_list_dist = 5;
                page.domains_from_seed = 3;
                page.times_seen = 1;

                // Estimate doc_len from max position
                for (size_t i = 0; i < page.word_positions.size(); i++) {
                    for (size_t j = 0; j < page.word_positions[i].size(); j++) {
                        if (page.word_positions[i][j] > page.doc_len)
                            page.doc_len = page.word_positions[i][j];
                    }
                }
                page.doc_len += 1;

                for (const NodeInfo& ni : phrases) {
                    if (url.contains(ni.phrase))
                        page.num_unique_words_found_url++;
                }
            }

            candidates.push_back(std::move(page));
        }

        return rank(candidates);
    }

    double getScore(DocInfo& dih) {
        init_query(dih);

        const string& url = dih.url;
        RankedPage page;
        page.url = string(url.data(), url.size());

        page.word_positions.reserve(dih.nodeInfo.size());
        for (NodeInfo& n : dih.nodeInfo) {
            page.word_positions.push_back(std::move(n.pos));
        }

        UrlData* data = url_store ? url_store->getUrl(url) : nullptr;
        if (data) {
            page.title = string(data->title.data(), data->title.size());
            page.seed_list_dist = data->seed_distance;
            page.domains_from_seed = data->domain_dist;
            page.times_seen = data->num_encountered;
            page.doc_len = data->eod;
            page.num_unique_words_found_anchor = count_anchor_term_matches(data);

            for (const NodeInfo& ni : dih.nodeInfo) {
                if (data->title.size() > 0 && data->title.contains(ni.phrase))
                    page.num_unique_words_found_title++;
                if (url.contains(ni.phrase))
                    page.num_unique_words_found_url++;
            }
        } else {
            page.seed_list_dist = 5;
            page.domains_from_seed = 3;
            page.times_seen = 1;
            for (size_t i = 0; i < page.word_positions.size(); i++)
                for (size_t j = 0; j < page.word_positions[i].size(); j++)
                    if (page.word_positions[i][j] > page.doc_len)
                        page.doc_len = page.word_positions[i][j];
            page.doc_len += 1;
        }

        return calc_static_score(page) * (1.0 - dynamic_weight) +
               calc_dynamic_score(page) * dynamic_weight;
    }
};
