#pragma once
#include "string.h"
#include <cstring>


// Encapsulation of data that we track as we crawl
struct CrawlTarget {
    string domain;          // Just the domain (stripped from full URL and free of `http://www` or `https://www.`
    string url;             // Entire URL
    uint16_t seed_distance; // URL hops from seed list
    uint16_t domain_dist;   // Domain hops from seed list
};


// Layout: [4: domain_len] [domain] [4: url_len] [url] [2: seed_distance] [2: domain_dist]

// Returns the total buffer size needed to serialize a CrawlTarget
inline size_t crawl_target_serialized_size(const CrawlTarget& ct) {
    return sizeof(uint32_t) + ct.domain.size()
         + sizeof(uint32_t) + ct.url.size()
         + sizeof(uint16_t) * 2;
}

// Serializes a CrawlTarget into buf, returns pointer past the written bytes
inline char* serialize_crawl_target(char* buf, const CrawlTarget& ct) {
    // domain (length-prefixed)
    uint32_t domain_len = static_cast<uint32_t>(ct.domain.size());
    std::memcpy(buf, &domain_len, sizeof(uint32_t));
    buf += sizeof(uint32_t);
    std::memcpy(buf, ct.domain.data(), ct.domain.size());
    buf += ct.domain.size();

    // url (length-prefixed)
    uint32_t url_len = static_cast<uint32_t>(ct.url.size());
    std::memcpy(buf, &url_len, sizeof(uint32_t));
    buf += sizeof(uint32_t);
    std::memcpy(buf, ct.url.data(), ct.url.size());
    buf += ct.url.size();

    // seed_distance
    std::memcpy(buf, &ct.seed_distance, sizeof(uint16_t));
    buf += sizeof(uint16_t);

    // domain_dist
    std::memcpy(buf, &ct.domain_dist, sizeof(uint16_t));
    buf += sizeof(uint16_t);

    return buf;
}

// Deserializes a CrawlTarget from buf, returns pointer past the read bytes
// Returns nullptr on failure (if remaining < expected)
inline const char* deserialize_crawl_target(const char* buf, size_t remaining, CrawlTarget& ct) {
    // domain
    if (remaining < sizeof(uint32_t)) return nullptr;
    uint32_t domain_len;
    std::memcpy(&domain_len, buf, sizeof(uint32_t));
    buf += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    if (remaining < domain_len) return nullptr;
    ct.domain = string(buf, domain_len);
    buf += domain_len;
    remaining -= domain_len;

    // url
    if (remaining < sizeof(uint32_t)) return nullptr;
    uint32_t url_len;
    std::memcpy(&url_len, buf, sizeof(uint32_t));
    buf += sizeof(uint32_t);
    remaining -= sizeof(uint32_t);

    if (remaining < url_len) return nullptr;
    ct.url = string(buf, url_len);
    buf += url_len;
    remaining -= url_len;

    // seed_distance + domain_dist
    if (remaining < sizeof(uint16_t) * 2) return nullptr;
    std::memcpy(&ct.seed_distance, buf, sizeof(uint16_t));
    buf += sizeof(uint16_t);
    std::memcpy(&ct.domain_dist, buf, sizeof(uint16_t));
    buf += sizeof(uint16_t);

    return buf;
}
