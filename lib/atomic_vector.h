#pragma once

#include <mutex>

#include "vector.h"

// USE AT UR OWN RISK I WROTE TS FOR MYSELF!!! -Esben
template <typename T>
class atomic_vector {
public:
    atomic_vector() = default;

    atomic_vector(const atomic_vector &) = delete;
    atomic_vector &operator=(const atomic_vector &) = delete;

    // Append a producer's local vector under a single lock.
    void append(const vector<T> &other) {
        std::lock_guard<std::mutex> lock(m);
        v.append_range(other.data(), other.data() + other.size());
    }

    // Move-append for non-copyable <T>
    void append_move(vector<T> &&other) {
        std::lock_guard<std::mutex> lock(m);
        for (size_t i = 0; i < other.size(); ++i) {
            v.push_back(move(other[i]));
        }
    }

    // Take underlying data
    vector<T> take() {
        std::lock_guard<std::mutex> lock(m);
        return move(v);
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(m);
        return v.size();
    }

private:
    std::mutex m;
    vector<T> v;
};
