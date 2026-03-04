#pragma once

#include "../lib/vector.h"
#include "../lib/string.h"
#include "lib/consts.h"
#include <cassert>


class BucketManager {
public:

    BucketManager(vector<string> bucket_files_in) {
        assert(bucket_files_in.size() == PRIORITY_BUCKETS);
    }

    // todo(hershey): write helper to load disk buckets into in-memory buckets

    // todo(hershey): write helper to persist in-memory buckets into disk buckets


private:
   
    vector<string> bucket_files;
};
