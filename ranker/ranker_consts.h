#pragma once

#include <cassert>
#include <cstddef>
#include "../lib/string.h"


// Approximation of Euler's number (used in exponential/logistic functions)
constexpr double e = 2.718; 


// Logistic function parameters for "times_seen" boost
constexpr double k = 0.06;   // Controls steepness of the sigmoid curve
constexpr double n_0 = 40;   // Midpoint: when times_seen ≈ 40 → score ≈ 0.5

// Exponential decay penalties for long fields
constexpr double Gamma_url   = 0.01;   // Penalizes long URLs
constexpr double Gamma_desc  = 0.0005; // (Unused currently) would penalize long descriptions
constexpr double Gamma_title = 0.01;   // Penalizes long titles


// DYNAMIC FACTORS (used in calc_dynamic_score)

// Factor 1: WORD PROXIMITY SCORE
// Measures how close query terms appear to each other in the document.
// Rewards tight clustering (phrase-like behavior).
constexpr double factor_1_weight = 7.0;

// Factor 2: TIMES SEEN (CRAWL FREQUENCY)
// Sigmoid-scaled score based on how often the page was encountered.
// Proxy for popularity / importance.
constexpr double factor_2_weight = 8.0;

// Factor 3: TITLE MATCH (RARITY-WEIGHTED)
// Measures how many query terms (especially rare ones) appear in the title.
// Penalized for long titles.
constexpr double factor_3_weight = 10.0;

// Factor 4: ANCHOR TEXT SIGNAL
// Combines:
//   - total incoming link frequency (volume)
//   - number of unique matching anchor phrases (diversity)
// Rewards pages widely referenced with relevant anchor text.
constexpr double factor_4_weight = 8.0;

// Factor 5: URL MATCH (RARITY-WEIGHTED)
// Checks if query terms appear in the URL string.
// Penalizes long URLs.
constexpr double factor_5_weight = 12.0;

// Factor 6: WORD FREQUENCY + RARITY COMBINED
// Measures how often query terms appear in the document,
// weighted by how rare (informative) those terms are globally.
constexpr double factor_6_weight = 8.0;

// Factor 7: EXACT PHRASE BONUS
// Rewards adjacent word matches (exact phrase occurrences).
// Boosts documents where query terms appear consecutively.
constexpr double factor_7_weight = 8.0;

// Factor 8: EARLY POSITION BONUS (rarity-weighted)
// Rewards query terms appearing early in the document,
// with stronger weight for rare terms.
constexpr double factor_8_weight = 6.0;

// Total sum for normalization
constexpr double dynamic_weight_sum =
    factor_1_weight + factor_2_weight + factor_3_weight +
    factor_4_weight + factor_5_weight + factor_6_weight +
    factor_7_weight + factor_8_weight;



// STATIC FACTORS (used in calc_static_score)

// Factor 1: DOMAIN DISTANCE FROM SEED
// Measures how "close" the domain is to trusted seed sites.
// Lower distance → higher trust.
constexpr double static_1_weight = 7.0;

// Factor 2: TLD QUALITY
// Boosts domains like .gov, .edu, etc.
// Penalizes unknown or low-quality TLDs.
constexpr double static_2_weight = 4.0;

// Factor 3: SEED LIST DISTANCE (PAGE LEVEL)
// Similar to Factor 1 but for the specific page instead of domain.
constexpr double static_3_weight = 1.0;

// Factor 4: DOMAIN LENGTH
// Shorter domain names are preferred (cleaner / more authoritative).
constexpr double static_4_weight = 3.0;

// Factor 5: SUBDOMAIN COUNT
// Penalizes URLs with many subdomains (often spammy or deep).
constexpr double static_5_weight = 3.0;

// Factor 6: DIGITS IN DOMAIN
// Penalizes domains with numbers (often low-quality/spam).
constexpr double static_6_weight = 2.0;

// Factor 7: URL LENGTH
// Shorter URLs are preferred.
constexpr double static_7_weight = 3.0;

// Factor 8: PATH DEPTH
// Penalizes deeply nested URLs (many `/` levels).
constexpr double static_8_weight = 4.0;

// Factor 9: QUERY PARAMETERS PENALTY
// Penalizes URLs containing '?' (dynamic/query URLs often lower quality).
constexpr double static_9_weight = 3.0;

// Controls exponential decay for seed distance scoring
constexpr double factor_3_const = -0.04;

// Total normalization weight for static scoring
constexpr double static_weight_sum =
    static_1_weight + static_2_weight + static_3_weight +
    static_4_weight + static_5_weight + static_6_weight +
    static_7_weight + static_8_weight + static_9_weight;



// Controls how strongly proximity affects the score
// Higher means proximity matters more
constexpr double LAMBDA_POS = 1.0;

// Controls saturation of term frequency contribution
// Higher means frequency saturates faster
constexpr double LAMBDA_FREQ = 0.2;


// FINAL SCORING BLEND
// Weight of dynamic score vs static score
// Final score = static*(1 - w) + dynamic*(w)
constexpr double DEFAULT_DYNAMIC_WEIGHT = 0.72;


// Number of top results to keep per node (SmallPQ size)
constexpr int RANKED_ON_EACH = 10;

// PROXIMITY WINDOW
// Maximum word distance considered "close"
// (~20 words ≈ one sentence)
constexpr size_t SENTENCE_WINDOW = 20;