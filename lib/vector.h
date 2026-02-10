#pragma once
#include <cassert>
#include <cstddef>                          // For size_t

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
        if (alloc_capacity > 0) delete[] alloc_region;
    }


    // Resize Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Constructs a vector with size num_elements,
    //    all default constructed
    vector(size_t num_elements) {
        alloc_region = new T[num_elements];
        alloc_capacity = num_elements;
        vec_size = num_elements;
    }


    // Fill Constructor
    // REQUIRES: Capacity > 0
    // MODIFIES: *this
    // EFFECTS: Creates a vector with size num_elements, all assigned to val
    vector(size_t num_elements, const T &val) {
        alloc_region = new T[num_elements];
        alloc_capacity = num_elements;
        vec_size = num_elements;

        for (size_t i = 0; i < vec_size; ++i) *(alloc_region + i) = val;
    }


    // Copy Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Creates a clone of the vector other
    vector(const vector<T> &other) {
        alloc_region = new T[other.capacity()];
        alloc_capacity = other.capacity();
        vec_size = other.size();

        for (size_t i = 0; i < vec_size; ++i) *(alloc_region + i) = other[i];
    }


    // Assignment operator
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Duplicates the state of other to *this
    vector operator=(const vector<T> &other) {
        if (this == &other) return *this;

        while (vec_size > 0) pop_back();
        for (size_t i = 0; i < other.size(); ++i) push_back(other[i]);

        return *this;
    }


    // Move Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this, leaves other in a default constructed state
    // EFFECTS: Takes the data from other into a newly constructed vector
    vector(vector<T> &&other) {
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
    vector operator=(vector<T> &&other) {
        if (this == &other) return *this;
        delete[] alloc_region;

        alloc_region = other.alloc_region;
        alloc_capacity = other.alloc_capacity;
        vec_size = other.vec_size;

        other.alloc_region = nullptr;
        other.alloc_capacity = 0;
        other.vec_size = 0;

        return *this;
    }


    // REQUIRES: new_capacity > capacity( )
    // MODIFIES: capacity( )
    // EFFECTS: Ensures that the vector can contain size( ) = new_capacity
    //    elements before having to reallocate
    void reserve(size_t newCapacity) { realloc_(newCapacity); }

    
    void resize(size_t newCapacity) {
        reserve(newCapacity);
        while(vec_size < newCapacity) {
            push_back(T()); // default construct new element
        }
    }


    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns the number of elements in the vector
    size_t size() const { return vec_size; }


    // Front Function
    // REQUIRES: At least one element
    // MODIFIES: Nothing
    // EFFECTS: Nothing
    T front() {
        assert(!empty());
        return alloc_region[0]; 
    }


    // Back Function
    // REQUIRES: At least one element
    // MODIFIES: Nothing
    // EFFECTS: Nothing
    T back() {
        assert(!empty());
        return alloc_region[vec_size - 1]; 
    }


    // Empty Function
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Nothing
    bool empty() const {
        return vec_size == 0;
    }


    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns the maximum size the vector can attain before resizing
    size_t capacity() const { return alloc_capacity; }


    // REQUIRES: 0 <= i < size( )
    // MODIFIES: Allows modification of data[i]
    // EFFECTS: Returns a mutable reference to the i'th element
    T &operator[](size_t i) { return alloc_region[i]; }


    // REQUIRES: 0 <= i < size( )
    // MODIFIES: Nothing
    // EFFECTS: Get a const reference to the ith element
    const T &operator[](size_t i) const { return alloc_region[i]; }


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

        *(alloc_region + vec_size) = x;
        vec_size++;
    }


    // REQUIRES: Nothing
    // MODIFIES: this, size( )
    // EFFECTS: Removes the last element of the vector,
    //    leaving capacity unchanged
    void pop_back() {
        assert(vec_size > 0);
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
        T* new_alloc_region = new T[new_alloc_capacity];
        for (size_t i = 0; i < vec_size; ++i) {
            *(new_alloc_region + i) = *(alloc_region + i);
        }

        delete[] alloc_region;
        alloc_region = new_alloc_region;
        alloc_capacity = new_alloc_capacity;
    }
};
