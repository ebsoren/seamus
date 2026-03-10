#pragma once

#include <cassert>
#include <iomanip>
#include <bit>
#include <stdexcept>
#include "lib/utils.h"

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
    Key& key;
    Value& value;

    Tuple( Key &k, Value &v ) : key( k ), value( v ) {}
};


// Default Hash functions and equality functors!
template<typename T>
struct DefaultHash {
    size_t operator()(const T& x) const {
        return (size_t)x; 
    }
};

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

    size_t uniqueKeys{};
    size_t map_capacity = 256;
    uint64_t collision_counter{};
    double loading_factor;

    Hash hash;
    Eq compareEqual;

    State* states;
    Key* keys;
    Value* values;
    // TODO(Aiden) : add a hash cache

    friend class HashBlob; // WILL NEED TO EDIT THIS TO WORK WITH THIS SPECIFIC MAP!


    void clear() {
        for (size_t i = 0; i < map_capacity; i++) {
            if (states[i] == State::FILLED) {
                keys[i].~Key();
                values[i].~Value();
            }
        }
    }

    void reallocate(size_t new_cap) {
        assert(valid_capacity(new_cap));

        auto* new_states = new State[new_cap]{};
        auto* new_keys = static_cast<Key*>(::operator new(new_cap*sizeof(Key)));
        auto* new_values = static_cast<Value*>(::operator new(new_cap*sizeof(Value)));

        collision_counter = 0;

        for (size_t index = 0; index < map_capacity; index++) {
            if(states[index] == State::FILLED) {
                size_t new_index = hash(keys[index]) & (new_cap - 1);
                while(new_states[new_index] == State::FILLED) {
                    new_index = (new_index + 1) & (new_cap - 1); // linear probing
                    collision_counter++;
                }
                new (new_keys + new_index) Key(::move(keys[index]));
                new (new_values + new_index) Value(::move(values[index]));
                new_states[new_index] = State::FILLED;
            }
        }

        clear();

        delete[] states;
        ::operator delete(keys);
        ::operator delete(values);

        map_capacity = new_cap;
        states = new_states;
        keys = new_keys;
        values = new_values;

    }


    // Finds the key in the table or an empty index
    size_t find_index(const Key& key) const {
        size_t index = hash(key) & (map_capacity - 1);
        const size_t start = index;
        size_t first_deleted = map_capacity;

        do {
            State state = states[index];
            if (state == State::EMPTY) {
                return (first_deleted != map_capacity) ? first_deleted : index;
            } else if (state == State::FILLED && compareEqual(keys[index], key)) {
                return index;
            } else if (state == State::DELETED and first_deleted == map_capacity) {
                first_deleted = index;
            }
            index = (index + 1) & (map_capacity - 1); // Linear Probing
        } while (index != start);

        if (first_deleted != map_capacity) {
            return first_deleted;
        }

        throw std::runtime_error("Table Full");
    }

    [[nodiscard]] bool valid_capacity(const size_t capacity) const noexcept {
        return capacity > uniqueKeys and (capacity & (capacity-1))==0;
    }

public:
    // an off-by-1 getter method that exists for const class methods/read-only access to map
    // todo(charlie): might migrate this into a constIterator class later
    const Value* get(const Key& key) const {
        const size_t index = find_index(key);

        if(states[index] == State::FILLED) {
            return &values[index];
        }

        return nullptr;
    }

    // INITIAL SIZE NEEDS TO BE A POWER OF 2!!!
    unordered_map(size_t capacity = 2048, double loading_factor = 0.65)
            : map_capacity((assert(valid_capacity(capacity)), capacity)), loading_factor(loading_factor) {
        states = new State[capacity]{};
        keys = static_cast<Key*>(::operator new(capacity*sizeof(Key)));
        values = static_cast<Value*>(::operator new(capacity*sizeof(Value)));
    }

    ~unordered_map() {
        // First call each object's destructor
        clear();
        // Then delete the objects
        delete[] states;
        ::operator delete(keys);
        ::operator delete(values);
    }

    void insert(const Key& key, const Value& value) {
        rehash(loading_factor);

        const size_t index = find_index(key);

        if(states[index] == State::FILLED) {
            values[index] = value;
        } else {
            states[index] = State::FILLED;
            new (keys + index) Key(key);
            new (values + index) Value(value);
            uniqueKeys++;
        }
    }

    bool erase(const Key& key) {
        const size_t index = find_index(key);
        if (states[index] == State::FILLED) {
            keys[index].~Key();
            values[index].~Value();

            states[index] = State::DELETED;
            uniqueKeys--;
            return true;
        }
        return false;
    }

    [[nodiscard]] size_t size() const noexcept {
        return uniqueKeys;
    }

    [[nodiscard]] size_t capacity() const noexcept {
        return map_capacity;
    }

    Value& operator[](const Key& key) {
        rehash(loading_factor);
        
        const size_t index = find_index(key);
        State state = states[index];

        if(state == State::EMPTY || state == State::DELETED) {
            states[index] = State::FILLED;
            new (keys + index) Key(key);
            new (values + index) Value{};
            uniqueKeys++;
            return values[index];
        }

        return values[index];
    }

    void rehash(const double loading) {
        // Grow the table given loading

        loading_factor = loading;

        if (uniqueKeys < loading_factor * map_capacity) {
            return;
        }
        // grow table by x2, if zero default to 256
        size_t new_cap = std::bit_ceil(static_cast<size_t>(uniqueKeys / loading_factor));
        new_cap = 256 > new_cap ? 256 : new_cap;

        reallocate(new_cap);
    }

    // SET A CUSTOM SIZE FOR THE MAP TO OPTIMIZE CAPACITY
    void reserve(const size_t size) {
        assert(size >= uniqueKeys);
        size_t new_cap = std::bit_ceil(static_cast<size_t>(size / loading_factor));

        reallocate(new_cap);
    }

    class Iterator {
        friend class unordered_map;

    private:
        unordered_map* map;
        size_t index;

        [[nodiscard]] size_t next_valid() const noexcept {
            for (size_t i = index; i < map->capacity(); i++) {
                if(map->states[i] == State::FILLED) {
                    return i;
                }
            }
            return map->capacity();
        }

        Iterator(unordered_map* m, const size_t i) : map(m), index(i) {
            index = next_valid();
        }

    public:
        explicit Iterator(unordered_map* m) : Iterator(m, 0) {}

        Iterator& operator++() {
            assert(index < map->capacity());
            ++index;
            index = next_valid();
            return *this;
        }

        Tuple<Key, Value> operator*() const {
            assert(index < map->capacity());
            return {map->keys[index], map->values[index]};
        }


        bool operator==(const Iterator& other) const {
            return index == other.index and map == other.map;
        }

        bool operator!=(const Iterator& other) const noexcept {
            return not (*this == other);
        }
    };

    Iterator begin() {
        return Iterator(this, 0);
    }

    Iterator end() {
        return Iterator(this, map_capacity);
    }

    Iterator find(const Key& key) {
        const size_t index = find_index(key);

        if(states[index] == State::FILLED) {
            return Iterator(this, index);
        }
        return end();
    }
};
