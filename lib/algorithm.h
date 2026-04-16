#pragma once

#include "string.h"
#include "utils.h"
#include "vector.h"

template<class It, class T>
inline bool binary_search(It first, It last, T& value) {

    while (first < last) {
        It mid = first + (last - first) / 2;

        if (!(*mid < value) && !(value < *mid)) return true;

        if  (*mid < value) {
            first = mid + 1;
        } else {
            last = mid;
        }
    }
    
    return false;
}

inline void radix_sort(vector<string> &vec, size_t l, size_t h, size_t idx) {
    if (h <= l) return;

    size_t lt = l, gt = h;

    // Get pivot character
    int p = charAt(vec[l], idx);

    size_t i = l + 1;
    while (i <= gt) {
        int c = charAt(vec[i], idx);

        if (c < p) {
            // If character is less than pivot, swap it to the low end of the vector
            swap(vec[lt++], vec[i++]);
        } else if (c > p) {
            // If greater, swap to the high end
            swap(vec[gt--], vec[i]);
        } else {
            // If character is the same, just continue to next character
            i++;
        }
    }

    // Recursively sort the 3 subarrays (guard against unsigned underflow)
    if (lt > l) radix_sort(vec, l, lt - 1, idx);
    radix_sort(vec, lt, gt, idx + 1); // This one is sorted by the next character
    if (gt + 1 <= h) radix_sort(vec, gt + 1, h, idx);
}

inline void radix_sort(vector<string> &vec) {
    if (vec.size() <= 1) return;
    radix_sort(vec, 0, vec.size() - 1, 0);
}

template <typename T>
inline constexpr const T& min(const T& a, const T& b) {
    // If b is strictly less than a, return b.
    // Otherwise, return a. 
    return (b < a) ? b : a;
}

template <typename T>
inline constexpr const T& max(const T& a, const T& b) {
    return (a < b) ? b : a;
}

template <class T, class Compare>
inline size_t partition(vector<T> &vec, size_t low, size_t high, Compare comp) {
    // std::sort often uses "Median-of-Three" for the pivot, but 
    // for now, we'll pick the middle element to avoid O(N^2) on sorted data.
    T pivot = move(vec[low + (high - low) / 2]);
    size_t i = low - 1;
    size_t j = high + 1;

    while (true) {
        do { i++; } while (comp(vec[i], pivot));
        do { j--; } while (comp(pivot, vec[j]));

        if (i >= j) return j;
        
        // Swap elements to their correct sides
        T temp = move(vec[i]);
        vec[i] = move(vec[j]);
        vec[j] = move(temp);
    }
}

template <class T, class Compare>
inline void quickSort(vector<T> &vec, size_t low, size_t high, Compare comp) {
    if (low < high) {
        size_t p = partition(vec, low, high, comp);
        quickSort(vec, low, p, comp);
        quickSort(vec, p + 1, high, comp);
    }
}

// Unused std::vector overload — kept here for reference but commented out
// because nothing includes <vector> at this point in the translation unit
// and gcc refuses to parse it (Mac clang gets it transitively). Re-enable
// by adding #include <vector> at the top of this file.
// template <class T, class Comp>
// inline void quickSort(std::vector<T>& vec, size_t low, size_t high, Comp comp) {
//     if (low < high) {
//         size_t p = partition(vec, low, high, comp);
//         quickSort(vec, low, p, comp);
//         quickSort(vec, p + 1, high, comp);
//     }
// }

template <class T, class Compare>
inline void sort(vector<T> &vec, Compare comp) {
    if (vec.size() <= 1) return;
    quickSort(vec, 0, vec.size() - 1, comp);
}