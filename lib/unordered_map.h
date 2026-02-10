#pragma once

#include "vector.h"
#include <cassert>
#include <iostream>
#include <vector>
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

// TODO: Could add a round-robin method for probing to help w time complexity
template< typename Key, typename Value > class unordered_map {
private:

    vector<Slot<Key, Value>> vec_map;
    uint64_t map_capacity;
    

    uint64_t ( *hash )( const Key );
    bool ( *compareEqual )( const Key, const Key );
    uint64_t uniqueKeys;
    uint64_t collision_counter;
    double loading_factor;


    // friend class Iterator; WILL NEED TO EDIT THIS TO WORK WITH THIS SPECIFIC MAP!
    friend class HashBlob; // WILL NEED TO EDIT THIS TO WORK WITH THIS SPECIFIC MAP!

public:
    // INITIAL SIZE NEEDS TO BE A POWER OF 2!!!
    unordered_map(size_t initialSize, uint64_t (*h)(const Key), bool (*eq)(const Key, const Key), double loading_factor_init = 0.65)
            : vec_map(initialSize), map_capacity(initialSize), hash(h), compareEqual(eq), uniqueKeys(0), collision_counter(0), loading_factor(loading_factor_init) { }

    Slot< Key, Value >* find( const Key& k, const Value initialValue ) {
        // Search for the key k and return a pointer to the
        // ( key, value ) entry.  If the key is not already
        // in the hash, add it with the initial value.

        // YOUR CODE HERE
        uint64_t index = hash(k) & (map_capacity - 1);
        
        uint64_t start = index;
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

        uint64_t index = hash(k) & (map_capacity - 1);

        uint64_t start = index;
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
        uint64_t index = hash(k) & (map_capacity - 1);

        uint64_t start = index;
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

    uint64_t size() {
        return uniqueKeys;
    }

    uint64_t capacity() {
        return map_capacity;
    }

    Value& operator[](const Key& k) {
        rehash(loading_factor);
        
        uint64_t index = hash(k) & (map_capacity - 1);
        uint64_t start = index;
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
            uint64_t new_cap = (map_capacity == 0 ? 256 : map_capacity * 2);

            new_slots.resize(new_cap);
            collision_counter = 0;
            
            for (Slot<Key, Value>& i : vec_map) {
                if(i.state == State::FILLED) {
                    uint64_t idx = hash(i.key) & (new_cap - 1);
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
    void optimize_size( uint64_t new_size ) {

        vector<Slot<Key, Value>> new_slots;
        uint64_t new_cap = new_size;

        new_slots.resize(new_cap);
        collision_counter = 0;
        
        for (Slot<Key, Value>& i : vec_map) {
            if(i.state == State::FILLED) {
                uint64_t idx = hash(i.key) & (new_cap - 1);
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
};
