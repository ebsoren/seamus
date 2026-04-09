#pragma once

#include <cstdint>

enum class MetricType : uint8_t {
    DOCUMENTS_CRAWLED_ACCUMULATE,           // Accumulator for total # of documents crawled for the entirety of the crawler runtime
    PAGE_LENGTH_AVERAGE,                    // Running average page length
    PAGE_PRIORITY_AVERAGE,                  // Running average of page priority

    LOCAL_URL_ACCUMULATE,                   // Accumulator for total # of locally found URLs
    RECEIVED_URL_ACCUMULATE,                // Accumulator for total # of URLs found and sent to us from other machines

    // TODO(hershey): implement if needed
    PAGE_LENGTH_DISTRIBUTION,               // Distribution buckets/intervals for page length
    PAGE_PRIORITY_DISTRIBUTION,             // Distribution buckets/intervals for page priority
};

struct MetricUpdate {
    MetricType type;
    double num;                 // numerator quantity
    int den;                    // denominator quantity (for averaging/batching)
};
