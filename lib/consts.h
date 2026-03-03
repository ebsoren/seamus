#pragma once

#include <cassert>
#include <cstddef>


// Global
constexpr size_t NUM_MACHINES = 1;                                  // todo(hershey): obviously, change when we deploy on more machines
constexpr const char* MACHINES[NUM_MACHINES] = {"127.0.0.1"};       // todo(hershey): replace localhost ip (127.0.0.1) with global ip of machines once we deploy on multiple machines -- store machine ID as an environment variable

inline const char* get_machine_addr(size_t machine_id) {
    // todo(hershey): once we deploy on multiple machines, check an environment variable here (e.g., self_id) and return localhost if machine_id == self_id

    assert(machine_id < NUM_MACHINES);
    return MACHINES[machine_id];
}


// Crawler
constexpr size_t CRAWLER_CAROUSEL_SIZE = 8192;
constexpr size_t CRAWLER_THREADPOOL_SIZE = 1024;
static_assert(CRAWLER_CAROUSEL_SIZE % CRAWLER_THREADPOOL_SIZE == 0, "[consts.h]: CRAWLER_CAROUSEL_SIZE must be a multiple of CRAWLER_THREADPOOL_SIZE");
static_assert(CRAWLER_CAROUSEL_SIZE >= CRAWLER_THREADPOOL_SIZE, "[consts.h]: CRAWLER_THREADPOOL_SIZE cannot be greater than CRAWLER_CAROUSEL_SIZE");

constexpr size_t PRIORITY_BUCKETS = 8;
