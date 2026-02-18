#include "string.h"
#include "utils.h"
#include "vector.h"

template<class It, class T>
bool binary_search(It first, It last, T& value) {

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

inline void radix_sort(vector<string> &vec) {
    radix_sort(vec, 0, vec.size() - 1, 0);
}

void radix_sort(vector<string> &vec, size_t l, size_t h, size_t idx) {
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

    // Recursively sort the 3 subarrays
    radix_sort(vec, l, lt - 1, idx);
    radix_sort(vec, lt, gt, idx + 1); // This one is sorted by the next character
    radix_sort(vec, gt + 1, h, idx);
}