#pragma once
#include <cassert>
#include <new>

template <typename T>
class deque {
public:

    // Default constructor
    deque() {
        assert(init_alloc_size % 2 != 0);

        // Initial allocation (raw memory, no constructors called)
        alloc_region = static_cast<T*>(::operator new(init_alloc_size * sizeof(T)));
        deque_capacity = init_alloc_size;
        deque_size = 0;

        // Set left and right pointers
        size_t padding_elts = (init_alloc_size - 1) / 2;
        deque_left = padding_elts;
        deque_right = padding_elts;
    }


    deque(const deque&) = delete;
    deque& operator=(const deque&) = delete;

    deque(deque&& other) noexcept
        : alloc_region(other.alloc_region),
          deque_capacity(other.deque_capacity),
          deque_size(other.deque_size),
          deque_left(other.deque_left),
          deque_right(other.deque_right) {
        other.alloc_region = nullptr;
        other.deque_capacity = 0;
        other.deque_size = 0;
    }

    deque& operator=(deque&& other) noexcept {
        if (this == &other) return *this;
        if (deque_capacity > 0) {
            if (deque_size > 0) {
                for (size_t i = deque_left; i <= deque_right; i++)
                    alloc_region[i].~T();
            }
            ::operator delete(alloc_region);
        }
        alloc_region = other.alloc_region;
        deque_capacity = other.deque_capacity;
        deque_size = other.deque_size;
        deque_left = other.deque_left;
        deque_right = other.deque_right;
        other.alloc_region = nullptr;
        other.deque_capacity = 0;
        other.deque_size = 0;
        return *this;
    }

    ~deque() {
        if (deque_capacity > 0) {
            // Destroy live elements
            if (deque_size > 0) {
                for (size_t i = deque_left; i <= deque_right; i++) {
                    alloc_region[i].~T();
                }
            }
            ::operator delete(alloc_region);
        }
    }


    // Push back (fills an element past the right pointer)
    void push_back(const T& val) {
        if (empty()) {
            new (alloc_region + deque_left) T(val);
            deque_size++;
            return;
        }
        if (deque_right == deque_capacity - 1) realloc_();
        deque_right++;
        deque_size++;
        new (alloc_region + deque_right) T(val);
    }


    // Push back (move)
    void push_back(T&& val) {
        if (empty()) {
            new (alloc_region + deque_left) T(static_cast<T&&>(val));
            deque_size++;
            return;
        }
        if (deque_right == deque_capacity - 1) realloc_();
        deque_right++;
        deque_size++;
        new (alloc_region + deque_right) T(static_cast<T&&>(val));
    }


    // Push front (fills an element before the left pointer)
    void push_front(const T& val) {
        if (empty()) {
            new (alloc_region + deque_left) T(val);
            deque_size++;
            return;
        }
        if (deque_left == 0) realloc_();
        deque_left--;
        deque_size++;
        new (alloc_region + deque_left) T(val);
    }


    // Push front (move)
    void push_front(T&& val) {
        if (empty()) {
            new (alloc_region + deque_left) T(static_cast<T&&>(val));
            deque_size++;
            return;
        }
        if (deque_left == 0) realloc_();
        deque_left--;
        deque_size++;
        new (alloc_region + deque_left) T(static_cast<T&&>(val));
    }


    void pop_back() {
        assert(!empty());
        alloc_region[deque_right].~T();

        if (deque_left == deque_right) {
            deque_size--;
            return;
        }

        deque_right--;
        deque_size--;
    }


    void pop_front() {
        assert(!empty());
        alloc_region[deque_left].~T();

        if (deque_left == deque_right) {
            deque_size--;
            return;
        }

        deque_left++;
        deque_size--;
    }


    T& front() { return *(alloc_region + deque_left); }


    T& back() { return *(alloc_region + deque_right); }


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
        T* new_alloc_region = static_cast<T*>(::operator new(new_deque_capacity * sizeof(T)));

        // Move existing elements into the middle of the new region
        size_t new_alloc_ptr = padding_size;
        for (size_t ptr = deque_left; ptr <= deque_right; ++ptr) {
            new (new_alloc_region + new_alloc_ptr) T(static_cast<T&&>(alloc_region[ptr]));
            alloc_region[ptr].~T();
            new_alloc_ptr++;
        }

        // Set new properties
        ::operator delete(alloc_region);
        alloc_region = new_alloc_region;
        deque_capacity = new_deque_capacity;
        deque_left = padding_size;
        deque_right = deque_left + deque_size - 1;
    }
};


template <typename T>
class atomic_queue {
    
};
