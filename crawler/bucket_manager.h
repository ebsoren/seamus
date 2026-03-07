#pragma once

#include "../lib/vector.h"
#include "../lib/string.h"
#include "../lib/deque.h"
#include "../lib/rpc_crawler.h"
#include "lib/consts.h"
#include <cassert>
#include <mutex>
#include <chrono>


struct BackoffEntry {
    CrawlTarget target;
    std::chrono::steady_clock::time_point rejected_at;
};


class BucketManager {
public:

    std::mutex backoff_lock;
    deque<BackoffEntry> backoff_queue;


    BucketManager(vector<string> bucket_files_in) : bucket_files(static_cast<vector<string>&&>(bucket_files_in)) {
        assert(bucket_files.size() == PRIORITY_BUCKETS);
    }


    static size_t get_priority_bucket(const string& url) {
        // TODO(hershey): delete this after Erik implements the same interface in Frontier, Frontier doesn't yet build and doesn't follow lib structure
        // 0 is the index of the highest priority bucket, PRIORITY_BUCKETS - 1 is the index of the lowest priority bucket
        // PRIORITY_BUCKETS defined in ~/lib/consts.h
        return 0;
    }


    // Sequentially spawns detached bucket manager routines (below)
    // Sleep 500ms between starting each routine:
    //      1) Load disk buckets into in-memory buckets
    //      2) Feed carousel
    //      3) Persist carousel
    void start() {

    }


    // Destructor
    ~BucketManager() {

    }


    // todo(hershey): write a detached, long-lived routine to feed the carousel from the buckets (using feed_carousel() as a util function)
    // This 1) tries to move crawl targets from priority buckets into the carousel and 2) tries to move crawl targets from the backoff queue into the carousel
    void feed_carousel_worker() {
        
    }


    // todo(hershey): write helper to load disk buckets into in-memory buckets
    // This should run ad-hoc as needed if in-memory buckets are empty


    // todo(hershey): write helper to persist in-memory buckets into disk buckets
    // This should run in a detached thread on an interval (PERSIST_INTERVAL_SEC in consts.h)


private:
  
    // bucket_files[priority] = file to serialized queue of urls
    // index 0 is highest priority, index PRIORITY_BUCKETS - 1 is the lowest
    vector<string> bucket_files;
};
