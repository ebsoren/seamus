#pragma once

#include "vector.h"
#include <cassert>
#include <iostream>
#include <iomanip>
#include <cstdint>

// int next_prime(uint64_t n) {
//     uint64_t candidate = n + 1;
    
//     if (candidate % 2 == 0) candidate++; 

//     while (!is_prime(candidate)) {
//         candidate += 2;
//     }

//     return candidate;
// }

// bool is_prime(uint64_t num) {
//     for (uint64_t i = 5; i * i <= num; i += 6) {
//         if (num % i == 0 || num % (i + 2) == 0)
//             return false;
//     }

// }

enum class State : uint8_t {
    EMPTY,
    FILLED,
    DELETED
};

template< typename Key, typename Value > class Tuple {
    public:
        Key key;
        Value value;

        Tuple( const Key &k, const Value &v ) : key( k ), value( v ) {}
};


template< typename Key, typename Value > class Slot {
    public:
        Key key;
        Value value;
        State state = State::EMPTY;

        Slot( const Key &k, const Value v ) : key(k), value(v) {
            state = State::FILLED;
        }
        Slot() : state(State::EMPTY) {}
};

// Default Hash functions and equality functors!
template<typename T>
struct DefaultHash {
    size_t operator()(const T& x) const {
        return (size_t)x; 
    }
};

#ifdef SEAMUS_STRING_H
template<>
struct DefaultHash<string> {
    size_t operator()(const string& s) const {
        size_t h = 14695981039346656037ULL;
        for (size_t i = 0; i < s.size(); ++i) {
            h ^= (unsigned char)s[i];
            h *= 1099511628211ULL;
        }
        return h;
    }
};
#endif

template<typename T>
struct DefaultEq {
    bool operator()(const T& a, const T& b) const {
        return a == b;
    }
};

// TODO: Could add a round-robin method for probing to help w time complexity
template<
    typename Key,
    typename Value,
    typename Hash = DefaultHash<Key>,
    typename Eq   = DefaultEq<Key>
> class unordered_map {
private:

    vector<Slot<Key, Value>> vec_map;
    size_t map_capacity;
    

    Hash hash;
    Eq compareEqual;
    size_t uniqueKeys;
    uint64_t collision_counter;
    double loading_factor;


    friend class Iterator; // WILL NEED TO EDIT THIS TO WORK WITH THIS SPECIFIC MAP!
    friend class HashBlob; // WILL NEED TO EDIT THIS TO WORK WITH THIS SPECIFIC MAP!

public:
    // INITIAL SIZE NEEDS TO BE A POWER OF 2!!!
    unordered_map(size_t initialSize = 2048, double loading_factor_init = 0.65)
            : vec_map(initialSize), map_capacity(initialSize), uniqueKeys(0), collision_counter(0), loading_factor(loading_factor_init) { }

    Slot< Key, Value >* find( const Key& k, const Value initialValue ) {
        // Search for the key k and return a pointer to the
        // ( key, value ) entry.  If the key is not already
        // in the hash, add it with the initial value.

        // YOUR CODE HERE
        size_t index = hash(k) & (map_capacity - 1);
        
        size_t start = index;
        do {
            Slot<Key, Value>& curr_val = vec_map[index];
            if(curr_val.state == State::EMPTY) {
                curr_val = Slot<Key, Value>(k, initialValue);
                uniqueKeys++;
                return &vec_map[index];
            } else if(curr_val.state == State::FILLED) {
                if(compareEqual(curr_val.key, k)) {
                    return &curr_val;
                }
            }
            index = (index + 1) & (map_capacity - 1); // linear probing
        } while(index != start);
        throw std::runtime_error("table full");
    }

    const Slot<Key, Value>* find(const Key& k) const {

        size_t index = hash(k) & (map_capacity - 1);

        size_t start = index;
        do {
            const Slot<Key, Value>& curr_val = vec_map[index];
            if(curr_val.state == State::EMPTY) {
                return nullptr; // replace with map.end()
            } else if(curr_val.state == State::FILLED) {
                if(compareEqual(curr_val.key, k)) {
                    return &curr_val;
                }
            }
            index = (index + 1) & (map_capacity - 1); // linear probing
        } while(index != start);
        throw std::runtime_error("table full");
    }

    Slot<Key, Value>* find(const Key& k) {

        size_t index = hash(k) & (map_capacity - 1);

        size_t start = index;
        do {
            const Slot<Key, Value>& curr_val = vec_map[index];
            if(curr_val.state == State::EMPTY) {
                return nullptr; // replace with map.end()
            } else if(curr_val.state == State::FILLED) {
                if(compareEqual(curr_val.key, k)) {
                    return &curr_val;
                }
            }
            index = (index + 1) & (map_capacity - 1); // linear probing
        } while(index != start);
        throw std::runtime_error("table full");
    }

    void insert(const Key& k, const Value& v) {
        size_t index = hash(k) & (map_capacity - 1);

        size_t start = index;
        do {
            Slot<Key, Value>& curr_val = vec_map[index];
            if(vec_map[index].state == State::FILLED) {
                if(compareEqual(k, vec_map[index].key)) {
                    vec_map[index].value = v;
                    return;
                }
            } else {
                curr_val = Slot<Key, Value>(k, v);
                uniqueKeys++;
                rehash(loading_factor);
                return;
            }
            index = (index + 1) & (map_capacity - 1);
            collision_counter++;
        } while(index != start);
        throw std::runtime_error("table full");
    }

    size_t size() {
        return uniqueKeys;
    }

    size_t capacity() {
        return map_capacity;
    }

    Value& operator[](const Key& k) {
        rehash(loading_factor);
        
        size_t index = hash(k) & (map_capacity - 1);
        size_t start = index;
        do {
            Slot<Key, Value>& curr_val = vec_map[index];
            if(curr_val.state == State::EMPTY) {
                curr_val.key = k;
                curr_val.state = State::FILLED;
                curr_val.value = Value{};
                uniqueKeys++;
                return curr_val.value;
            } else if(curr_val.state == State::FILLED) {
                if(compareEqual(curr_val.key, k)) {
                    return curr_val.value;
                }
            }
            index = (index + 1) & (map_capacity - 1); // linear probing
        } while(index != start);
        throw std::runtime_error("table full");
    }

    void rehash( double loading ) {
        // Grow or shrink the table as appropriate once we know the loading. A
        // goodrule of thumb is that the table size should be at least 1.5x the
        // number of unique keys.

        // YOUR CODE HERE
        loading_factor = loading;

        bool is_overloaded = (double)uniqueKeys / map_capacity >= loading_factor;

        if (is_overloaded) {
            // grow table by x2
            vector<Slot<Key, Value>> new_slots;
            size_t new_cap = (map_capacity == 0 ? 256 : map_capacity * 2);

            new_slots.resize(new_cap);
            collision_counter = 0;
            
            for (Slot<Key, Value>& i : vec_map) {
                if(i.state == State::FILLED) {
                    size_t idx = hash(i.key) & (new_cap - 1);
                    while(new_slots[idx].state == State::FILLED) {
                        idx = (idx + 1) & (new_cap - 1); // linear probing
                        collision_counter++;
                    }
                    new_slots[idx] = Slot<Key, Value>(i.key, i.value);
                }
            }
            map_capacity = new_cap;
            vec_map = new_slots;
        }
    }
    // SET A CUSTOM SIZE FOR THE MAP TO OPTIMIZE!
    void optimize_size( size_t new_size ) {

        vector<Slot<Key, Value>> new_slots;
        size_t new_cap = new_size;

        new_slots.resize(new_cap);
        collision_counter = 0;
        
        for (Slot<Key, Value>& i : vec_map) {
            if(i.state == State::FILLED) {
                size_t idx = hash(i.key) & (new_cap - 1);
                while(new_slots[idx].state == State::FILLED) {
                    idx = (idx + 1) & (new_cap - 1); // linear probing
                    collision_counter++;
                }
                new_slots[idx] = Slot<Key, Value>(i.key, i.value);
            }
        }
        map_capacity = new_cap;
        vec_map = new_slots;
    } // new size must be a power of 2!!

    class Iterator {
    private:
        const unordered_map* map;
        size_t index;

        void next_valid() {
            while(index < map->map_capacity) {
                if(map->vec_map[index].state == State::FILLED) {
                    return;
                }
                index++;
            }
        }
    public:
        Iterator(unordered_map* m, size_t i)
            : map(m), index(i)
        {
            next_valid();
        }

        Iterator& operator++() {
            ++index;
            next_valid();
            return *this;
        }

        const Slot<Key, Value>& operator*() {
            return map->vec_map[index];
        }

        const Slot<Key, Value>& operator*() const {
            return map->vec_map[index];
        }

        Slot<Key, Value>* operator->() {
            return &map->vec_map[index];
        }

        const Slot<Key, Value>* operator->() const {
            return &map->vec_map[index];
        }

        bool operator!=(const Iterator& other) const {
            return index != other.index;
        }

        bool operator==(const Iterator& other) const {
            return index == other.index;
        }
    };

    Iterator begin() {
        return Iterator(this, 0);
    }

    Iterator end() {
        return Iterator(this, map_capacity);
    }
};
