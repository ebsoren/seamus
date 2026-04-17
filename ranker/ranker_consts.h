#pragma once

#include <cassert>
#include <cstddef>
#include "../lib/string.h"

// PARAMETERS FOR THE DYNAMIC WEIGHTING FUNCTION. TUNE TO MAKE IT BETTER.
constexpr double e = 2.718; 
constexpr double k = 0.06; // this controls sharpness of the graph
constexpr double n_0 = 40; // this is where it equals 0.5
constexpr double Gamma_url = 0.01; 
constexpr double Gamma_desc = 0.0005; 
constexpr double Gamma_title = 0.01; 
constexpr double factor_1_weight = 7.0;
constexpr double factor_2_weight = 4.0;
constexpr double factor_3_weight = 10.0;
constexpr double factor_4_weight = 4.0;
constexpr double factor_5_weight = 15.0;
constexpr double factor_6_weight = 7.0;
constexpr double factor_7_weight = 8.0;  // exact phrase adjacency bonus
constexpr double dynamic_weight_sum = factor_1_weight + factor_2_weight + factor_3_weight + factor_4_weight + factor_5_weight +
    factor_6_weight + factor_7_weight;

// PARAMETERS FOR THE STATIC WEIGHTING FUNCTION. TUNE TO MAKE IT BETTER.
constexpr double static_1_weight = 7.0;
constexpr double static_2_weight = 4.0;
constexpr double static_3_weight = 1.0;
constexpr double static_4_weight = 3.0;
constexpr double static_5_weight = 3.0;
constexpr double static_6_weight = 2.0;
constexpr double static_7_weight = 3.0;
constexpr double static_8_weight = 4.0;
constexpr double static_9_weight = 3.0;
constexpr double factor_3_const = -0.04;
constexpr double static_weight_sum = static_1_weight + static_2_weight + static_3_weight + static_4_weight + 
    static_5_weight + static_6_weight + static_7_weight + static_8_weight + static_9_weight;

// PARAMS FOR WORD POS SCORE
constexpr double LAMBDA_POS = 1.0; // modify this factor during tuning

// PARAMS FOR WORD FREQ AND RARITY SCORE
constexpr double LAMBDA_FREQ = 0.2;

// PARAM FOR DYNAMIC VS STATIC WEIGHTING
constexpr double DEFAULT_DYNAMIC_WEIGHT = 0.72;

// PARAM FOR AMOUNT OF RESULTS WE WANT TO RETURN ON EACH MACHINE
constexpr int RANKED_ON_EACH = 10;

// Define the maximum distance to be considered "in close proximity" (e.g., ~1 sentence)
constexpr size_t SENTENCE_WINDOW = 20;
