#pragma once
#include <cassert>
#include <cstddef>                          // For size_t
#include <new>                              // For placement new
#include "utils.h"

template<typename T>
class vector {
public:
    using value_type = T;
    // Default Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Constructs an empty vector with capacity 0
    vector() {
        alloc_region = nullptr;
        alloc_capacity = 0;
        vec_size = 0;
    }


    // Destructor
    // REQUIRES: Nothing
    // MODIFIES: Destroys *this
    // EFFECTS: Performs any neccessary clean up operations
    ~vector() {
        for (size_t i = 0; i < vec_size; ++i) alloc_region[i].~T();
        if (alloc_capacity > 0) ::operator delete(alloc_region);
    }


    // Resize Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Constructs a vector with size num_elements,
    //    all default constructed
    vector(size_t num_elements) {
        alloc_region = static_cast<T*>(::operator new(num_elements * sizeof(T)));
        alloc_capacity = num_elements;
        vec_size = 0;
        for (size_t i = 0; i < num_elements; ++i) {
            new (alloc_region + i) T();
            ++vec_size;
        }
    }


    // Fill Constructor
    // REQUIRES: Capacity > 0
    // MODIFIES *this
    // EFFECTS: Creates a vector with size num_elements, all assigned to val
    vector(size_t num_elements, const T &val) {
        alloc_region = static_cast<T*>(::operator new(num_elements * sizeof(T)));
        alloc_capacity = num_elements;
        vec_size = 0;
        for (size_t i = 0; i < num_elements; ++i) {
            new (alloc_region + i) T(val);
            ++vec_size;
        }
    }

    void clear() {
        for (size_t i = 0; i < vec_size; ++i) {
            alloc_region[i].~T();
        }
        vec_size = 0;
    }


    // Copy Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Creates a clone of the vector other
    vector(const vector<T> &other) {
        alloc_region = static_cast<T*>(::operator new(other.capacity() * sizeof(T)));
        alloc_capacity = other.capacity();
        vec_size = 0;
        for (size_t i = 0; i < other.size(); ++i) {
            new (alloc_region + i) T(other[i]);
            ++vec_size;
        }
    }


    // Assignment operator
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Duplicates the state of other to *this
    vector& operator=(const vector<T> &other) {
        if (this == &other) return *this;

        while (vec_size > 0) pop_back();
        for (size_t i = 0; i < other.size(); ++i) push_back(other[i]);

        return *this;
    }


    // Move Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this, leaves other in a default constructed state
    // EFFECTS: Takes the data from other into a newly constructed vector
    vector(vector<T> &&other) noexcept {
        alloc_region = other.alloc_region;
        alloc_capacity = other.alloc_capacity;
        vec_size = other.vec_size;

        other.alloc_region = nullptr;
        other.alloc_capacity = 0;
        other.vec_size = 0;
    }


    // Move Assignment Operator
    // REQUIRES: Nothing
    // MODIFIES: *this, leaves otherin a default constructed state
    // EFFECTS: Takes the data from other in constant time
    vector& operator=(vector<T> &&other) noexcept {
        if (this == &other) return *this;
        for (size_t i = 0; i < vec_size; ++i) alloc_region[i].~T();
        if (alloc_capacity > 0) ::operator delete(alloc_region);

        alloc_region = other.alloc_region;
        alloc_capacity = other.alloc_capacity;
        vec_size = other.vec_size;

        other.alloc_region = nullptr;
        other.alloc_capacity = 0;
        other.vec_size = 0;

        return *this;
    }


    // REQUIRES: Nothing
    // MODIFIES: capacity( )
    // EFFECTS: Ensures that the vector can contain size( ) = new_capacity
    //    elements before having to reallocate
    void reserve(size_t newCapacity) {
        if (newCapacity <= alloc_capacity) return;
        realloc_(newCapacity);
    }


    void resize(size_t newCapacity) {
        if (newCapacity < vec_size) {
            // Shrink: destroy excess elements
            for (size_t i = newCapacity; i < vec_size; ++i) {
                alloc_region[i].~T();
            }
            vec_size = newCapacity;
        } else if (newCapacity > vec_size) {
            // Grow: allocate and default construct
            reserve(newCapacity);
            while(vec_size < newCapacity) {
                new (alloc_region + vec_size) T();
                vec_size++;
            }
        }
    }


    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns the number of elements in the vector
    [[nodiscard]] size_t size() const noexcept { return vec_size; }


    // Front Function
    // REQUIRES: At least one element
    // MODIFIES: Nothing
    // EFFECTS: Nothing
    T& front() {
        assert(!empty());
        return alloc_region[0];
    }

    const T& front() const{
        assert(!empty());
        return alloc_region[0];
    }


    // Back Function
    // REQUIRES: At least one element
    // MODIFIES: Nothing
    // EFFECTS: Nothing
    T& back() {
        assert(!empty());
        return alloc_region[vec_size - 1];
    }

    const T& back() const{
        assert(!empty());
        return alloc_region[vec_size - 1];
    }


    // Empty Function
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Nothing
    [[nodiscard]] bool empty() const noexcept {
        return vec_size == 0;
    }


    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns the maximum size the vector can attain before resizing
    [[nodiscard]] size_t capacity() const noexcept { return alloc_capacity; }


    // REQUIRES: 0 <= i < size( )
    // MODIFIES: Allows modification of data[i]
    // EFFECTS: Returns a mutable reference to the i'th element
    T &operator[](size_t i) noexcept { return alloc_region[i]; }


    // REQUIRES: 0 <= i < size( )
    // MODIFIES: Nothing
    // EFFECTS: Get a const reference to the ith element
    const T &operator[](size_t i) const noexcept { return alloc_region[i]; }


    // REQUIRES: Nothing
    // MODIFIES: this, size( ), capacity( )
    // EFFECTS: Appends the element x to the vector, allocating
    //    additional space if neccesary
    void push_back(const T &x) {
        if (vec_size == alloc_capacity) {
            size_t new_alloc_capacity = 1;
            if (vec_size > 0) new_alloc_capacity = vec_size * REALLOC_FACTOR;
            realloc_(new_alloc_capacity);
        }

        new (alloc_region + vec_size) T(x);
        vec_size++;
    }


    // REQUIRES: Nothing
    // MODIFIES: this, size( ), capacity( )
    // EFFECTS: Appends the element x to the vector by move, allocating
    //    additional space if neccesary
    void push_back(T &&x) {
        if (vec_size == alloc_capacity) {
            size_t new_alloc_capacity = 1;
            if (vec_size > 0) new_alloc_capacity = vec_size * REALLOC_FACTOR;
            realloc_(new_alloc_capacity);
        }

        new (alloc_region + vec_size) T(::move(x));
        vec_size++;
    }


    // REQUIRES: Nothing
    // MODIFIES: this, size( )
    // EFFECTS: Removes the last element of the vector,
    //    leaving capacity unchanged
    void pop_back() {
        assert(vec_size > 0);
        alloc_region[vec_size - 1].~T();
        vec_size--;
    }


    // REQUIRES: Nothing
    // MODIFIES: Allows mutable access to the vector's contents
    // EFFECTS: Returns a mutable random access iterator to the
    //    first element of the vector
    T* begin() { return alloc_region; }


    // REQUIRES: Nothing
    // MODIFIES: Allows mutable access to the vector's contents
    // EFFECTS: Returns a mutable random access iterator to
    //    one past the last valid element of the vector
    T* end() { return alloc_region + vec_size; }


    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns a random access iterator to the first element of the vector
    const T* begin() const { return alloc_region; }


    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns a random access iterator to
    //    one past the last valid element of the vector
    const T* end() const { return alloc_region + vec_size; }


private:

    T* alloc_region;                // Existing heap allocated region
    size_t alloc_capacity;          // Capacity of currently allocated region (in terms of type T)
    size_t vec_size;                // Current size of vector
    size_t REALLOC_FACTOR = 2;      // Factor by which to grow the heap allocated region on reallocation

    // Reallocates `alloc_region` to a specified size and transfers elements into the new region
    void realloc_(size_t new_alloc_capacity) {
        T* new_alloc_region = static_cast<T*>(::operator new(new_alloc_capacity * sizeof(T)));
        for (size_t i = 0; i < vec_size; ++i) {
            new (new_alloc_region + i) T(::move(alloc_region[i]));
            alloc_region[i].~T();
        }

        if (alloc_capacity > 0) ::operator delete(alloc_region);
        alloc_region = new_alloc_region;
        alloc_capacity = new_alloc_capacity;
    }
};
