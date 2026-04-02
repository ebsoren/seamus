
#include <cstddef>
#include <initializer_list>

#include <stdlib.h>

template <typename T, std::size_t N>
struct array {

    // Provide const and non-const methods to allow use on any array
    // first const indicates result cannot be modified; second const means the
    // function may be called on a const data type.

    T &front() { return data_[0]; }
    T &back() { return data_[N - 1]; }

    T *data() { return data_; }

    const T &front() const { return data_[0]; }
    const T &back() const { return data_[N - 1]; }

    const T *data() const { return data_; }

    T *begin() { return data_; }
    T *end() { return data_ + N; }

    const T *begin() const { return data_; }
    const T *end() const { return data_ + N; }

    constexpr std::size_t size() const { return N; }

    T &operator[](std::size_t i) { return data_[i]; }

    const T &operator[](size_t i) const { return data_[i]; }

    private: 

    T data_[N];
    
};
