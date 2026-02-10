/*
    TODO(hershey):

    - Constructor that initializes capacity at any size
    - Reallocations if the deque size falls significantly below the deque capacity (to free heap memory)
            ^^ Note that this happens implicitly when push_back() and push_front() need to realloc_()
*/

#pragma once
#include <cassert>

template <typename T>
class deque {
public:

    // Default constructor
    deque() {
        assert(init_alloc_size % 2 != 0);

        // Initial allocation
        alloc_region = new T[init_alloc_size];
        deque_capacity = init_alloc_size;
        deque_size = 0;

        // Set left and right pointers
        size_t padding_elts = (init_alloc_size - 1) / 2;
        deque_left = padding_elts;
        deque_right = padding_elts;
    }


    // Destructor
    ~deque() {
        if (deque_capacity > 0) delete[] alloc_region;
    }


    // Push back (fills an element past the right pointer)
    void push_back(const T& val) {
        // If currently empty, just set the val
        if (empty()) {
            *(alloc_region + deque_left) = val;
            deque_size++;
            return;
        }

        // Check if we need to reallocate (this also adjusts left/right pointers)
        if (deque_right == deque_capacity - 1) realloc_();

        // Move right pointer to new valid region and fill
        deque_right++;
        deque_size++;
        *(alloc_region + deque_right) = val;
    }


    // Push front (fills an element before the left pointer)
    void push_front(const T& val) {
        // If currently empty, just set the val
        if (empty()) {
            *(alloc_region + deque_left) = val;
            deque_size++;
            return;
        }

        // Check if we need to reallocate (this also adjusts left/right pointers)
        if (deque_left == 0) realloc_();

        // Move left pointer to new valid region and fill
        deque_left--;
        deque_size++;
        *(alloc_region + deque_left) = val;
    }


    void pop_back() {
        assert(!empty());

        if (deque_left == deque_right) {
            deque_size--;
            return;
        }

        deque_right--;
        deque_size--;
    }


    void pop_front() {
        assert(!empty());

        if (deque_left == deque_right) {
            deque_size--;
            return;
        }

        deque_left++;
        deque_size--;
    }


    T* front() { return (alloc_region + deque_left); }


    T* back() { return (alloc_region + deque_right); }


    size_t size() { return deque_size; }


    bool empty() { return deque_size == 0; }


    // O(1) access!!!
    T& operator[](size_t i) { return *(alloc_region + deque_left + i); }


private:

    T* alloc_region;                // Underlying heap allocation for the deque
    size_t deque_capacity;          // Capacity of the deque before reallocation is needed (in terms of type T)
    size_t deque_size;              // Size of the valid portion of the deque (region with elements of type T)
    size_t deque_left;              // Left bound on the valid portion of the deque (inclusive)
    size_t deque_right;             // Right bound on the valid portion of the deque (inclusive)
    size_t init_alloc_size = 9;     // Initial size of the heap region (allocated in terms of type T) -- should be an odd number


    // Reallocates the heap region to be double the current deque size, calculates padding, fills the middle of the new region, and sets left/right deque pointers accordingly
    void realloc_() {
        // Add (deque size / 2) + 1 padding to the left and right in the new region
        size_t padding_size = (deque_size / 2) + 1;
        size_t new_deque_capacity = (2 * padding_size) + deque_size;
        T* new_alloc_region = new T[new_deque_capacity];

        // Copy existing elements into the middle of the new region
        size_t new_alloc_ptr = padding_size;
        for (size_t ptr = deque_left; ptr <= deque_right; ++ptr) {
            *(new_alloc_region + new_alloc_ptr) = *(alloc_region + ptr);
            new_alloc_ptr++;
        }

        // Set new properties
        delete[] alloc_region;
        alloc_region = new_alloc_region;
        deque_capacity = new_deque_capacity;
        deque_left = padding_size;
        deque_right = deque_left + deque_size - 1;
    }
};
