#pragma once

#include "../lib/deque.h"
#include "../lib/string.h"
#include "../lib/consts.h"
#include <mutex>


struct CrawlTarget {
    string domain;          // Just the domain (stripped from full URL and free of `http://www` or `https://www.`
    string url;             // Entire URL
};


class DomainCarousel {

};
