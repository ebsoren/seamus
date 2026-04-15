#pragma once

#include <cassert>
#include <cstddef>
#include "../lib/string.h"

// PARAMETERS FOR THE DYNAMIC WEIGHTING FUNCTION. TUNE TO MAKE IT BETTER.
constexpr double e = 2.718; 
constexpr double k = 0.25; // this controls sharpness of the graph
constexpr double n_0 = 5; // this is where it equals 0.5
constexpr double Gamma_url = 0.1; 
constexpr double Gamma_desc = 0.0005; 
constexpr double Gamma_title = 0.2; 
constexpr double factor_1_weight = 5.0;
constexpr double factor_2_weight = 1.0;
constexpr double factor_3_weight = 1.0;
constexpr double factor_4_weight = 1.0;
constexpr double factor_5_weight = 1.0;
constexpr double factor_6_weight = 1.0;
constexpr double dynamic_weight_sum = factor_1_weight + factor_2_weight + factor_3_weight + factor_4_weight + factor_5_weight + 
    factor_6_weight;

// PARAMETERS FOR THE STATIC WEIGHTING FUNCTION. TUNE TO MAKE IT BETTER.
constexpr double static_1_weight = 1.0;
constexpr double static_2_weight = 1.0;
constexpr double static_3_weight = 1.0;
constexpr double static_4_weight = 1.0;
constexpr double static_5_weight = 1.0;
constexpr double static_6_weight = 1.0;
constexpr double static_7_weight = 1.0;
constexpr double static_8_weight = 1.0;
constexpr double static_9_weight = 1.0;
constexpr double static_weight_sum = static_1_weight + static_2_weight + static_3_weight + static_4_weight + 
    static_5_weight + static_6_weight + static_7_weight + static_8_weight + static_9_weight;


// PARAM FOR DYNAMIC VS STATIC WEIGHTING
constexpr double DEFAULT_DYNAMIC_WEIGHT = 0.67;

// PARAM FOR AMOUNT OF RESULTS WE WANT TO RETURN ON EACH MACHINE
constexpr int RANKED_ON_EACH = 10;