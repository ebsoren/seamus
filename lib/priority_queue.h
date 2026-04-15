#pragma once
#include <cassert>
#include <cstddef>                          // For size_t
#include "vector.h"


template <typename T>
struct less {
    // returns true if a is less than b (default comparator)
    constexpr bool operator()(const T& a, const T& b) const {
        return a < b;
    }
};


// template <typename T>
// void swap(T& a, T& b) {
//     // swaps two values using std::move
//     T temp = std::move(a);
//     a = std::move(b);
//     b = std::move(temp);
// }


template <class InputIter, class Compare>
void heap_sort_down(InputIter first, size_t n, size_t p, Compare comp) {
    // restores heap property downward from index p in range [first, first+n) 
    // decided to overload this function so it can be used with iterators as well
    while (true) {
        size_t left  = 2 * p + 1;
        size_t right = 2 * p + 2;
        size_t best = p;

        if (left < n && comp(*(first + best), *(first + left)))
            best = left;

        if (right < n && comp(*(first + best), *(first + right)))
            best = right;

        if (best == p) break;

        swap(*(first + p), *(first + best));
        p = best;
    }
}


template <class InputIter, class Compare>
void heapify(InputIter first, InputIter last, Compare comp) {
    // builds a heap in-place from range [first, last) in O(n)
    auto n = last - first;

    if (n <= 1) {
        return;
    }

    size_t p = (n / 2) - 1;
    do {
        heap_sort_down(first, n, p, comp);
    } while(p-- != 0);
}


template<
    class T, class Container = vector<T>, class Compare = less<typename Container::value_type>
>
class priority_queue {
public:
    // default constructs an empty priority queue
    priority_queue() : heap(), comp() { }


    // constructs from an existing container and builds heap
    priority_queue(const Compare& compare, Container&& cont) : heap(std::move(cont)), comp(compare) {
        heapify(heap.begin(), heap.end(), comp);
    }

    priority_queue(const Compare& compare) : heap(), comp(compare) {}


    // constructs from a range and optional container + comparator
    template <class InputIter>
    priority_queue(InputIter first, InputIter last, const Compare& compare = Compare(), Container&& cont = Container()) : heap(std::move(cont)), comp(compare) {
        for (; first != last; ++first) {
            heap.push_back(*first);
        }
        heapify(heap.begin(), heap.end(), comp);
    }


    // adds an element and restores heap order by sifting up
    void push(const T& elt) {
        heap.push_back(elt);
        heap_sort_up();
    }

    void push(T&& elt) {
        // std::move it into the underlying vector
        heap.push_back(std::move(elt));
        
        heap_sort_up(); 
    }


    // removes the top element and restores heap order by sifting down
    void pop() {
        assert(!empty());
        heap[0] = std::move(heap.back());   // move last element to root so we can remove using popBack
        heap.pop_back(); 
                            
        if(!heap.empty()) {
            heap_sort_down(0); 
        }
    }

    T pop_move() {
        assert(!empty());

        T top_elem = std::move(heap[0]);

        heap[0] = std::move(heap.back());
        heap.pop_back();

        if (!heap.empty()) {
            heap_sort_down(0);
        }
        return top_elem;
    }

    void clear() {
        heap.clear();
    }


    // returns number of elements in heap
    size_t size() const { return heap.size(); }


    // returns reference to top element (highest priority)
    const T& front() const {
        assert(!empty());
        return heap[0];
    }


    // checks whether heap is empty
    bool empty() const { return heap.empty(); }

    class Iterator {
    private:
        const priority_queue* pq;
        size_t index;

    public:
        Iterator(priority_queue* p, size_t i)
            : pq(p), index(i) { }

        Iterator& operator++() {
            ++index;
            return *this;
        }

        T operator*() {
            return pq->heap[index];
        }

        const T& operator*() const {
            return pq->heap[index];
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
        return Iterator(this, heap.size());
    }


private:
    // restores heap order upward from the last element
    void heap_sort_up() {
        size_t i = heap.size() - 1;
        if(i == 0) {
            return;
        }

        while (i != 0) {
            size_t p = (i - 1) / 2;

            if (comp(heap[p], heap[i])) {
                swap(heap[p], heap[i]);
                i = p;
            } else {
                break;
            }
        }
    }


    // restores heap order downward from index p
    void heap_sort_down(size_t p = 0) {
        size_t n = heap.size();

        while (true) {
            size_t c1 = 2 * p + 1;
            size_t c2 = 2 * p + 2;
            size_t best = p;

            if (c1 < n && comp(heap[best], heap[c1])) best = c1;
            if (c2 < n && comp(heap[best], heap[c2])) best = c2;

            if (best == p) break;

            swap(heap[p], heap[best]);
            p = best;
        }
    }


    Container heap;                        // underlying heap storage (defaults to max heap similarly to stl)
    Compare comp;                          // comparator object which is customizable

    friend class Iterator;
};