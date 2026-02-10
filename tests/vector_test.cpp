#include "../lib/vector.h"
#include <cassert>

// Test default constructor
void test_default_constructor()
{
    vector<int> v;
    assert(v.size() == 0);
    assert(v.capacity() == 0);
}

// Test resize constructor
void test_resize_constructor()
{
    vector<int> v(5);
    assert(v.size() == 5);
    assert(v.capacity() == 5);
}

// Test fill constructor
void test_fill_constructor()
{
    vector<int> v(5, 42);
    assert(v.size() == 5);
    assert(v.capacity() == 5);
    for (size_t i = 0; i < v.size(); ++i)
    {
        assert(v[i] == 42);
    }
}

// Test copy constructor
void test_copy_constructor()
{
    vector<int> v1(3, 10);
    vector<int> v2(v1);

    assert(v2.size() == v1.size());
    assert(v2.capacity() == v1.capacity());
    for (size_t i = 0; i < v1.size(); ++i)
    {
        assert(v2[i] == v1[i]);
    }

    // Verify deep copy
    v2[0] = 99;
    assert(v1[0] == 10);
    assert(v2[0] == 99);
}

// Test assignment operator
void test_assignment_operator()
{
    vector<int> v1(3, 10);
    vector<int> v2;

    v2 = v1;
    assert(v2.size() == v1.size());
    for (size_t i = 0; i < v1.size(); ++i)
    {
        assert(v2[i] == v1[i]);
    }

    // Test self-assignment
    v1 = v1;
    assert(v1.size() == 3);
    assert(v1[0] == 10);
}

// Test move constructor
void test_move_constructor()
{
    vector<int> v1(3, 42);
    int *old_ptr = v1.begin();

    vector<int> v2(static_cast<vector<int> &&>(v1));

    assert(v2.size() == 3);
    assert(v2.capacity() == 3);
    assert(v2[0] == 42);
    assert(v2.begin() == old_ptr);

    assert(v1.size() == 0);
    assert(v1.capacity() == 0);
    assert(v1.begin() == nullptr);
}

// Test move assignment operator
void test_move_assignment_operator()
{
    vector<int> v1(3, 42);
    vector<int> v2(2, 99);
    int *old_ptr = v1.begin();

    v2 = static_cast<vector<int> &&>(v1);

    assert(v2.size() == 3);
    assert(v2.capacity() == 3);
    assert(v2[0] == 42);
    assert(v2.begin() == old_ptr);

    assert(v1.size() == 0);
    assert(v1.capacity() == 0);
    assert(v1.begin() == nullptr);

    // Test self-assignment
    vector<int> v3(3, 7);
    v3 = static_cast<vector<int> &&>(v3);
    assert(v3.size() == 3);
}

// Test push_back
void test_push_back()
{
    vector<int> v;

    // Push to empty vector
    v.push_back(1);
    assert(v.size() == 1);
    assert(v.capacity() == 1);
    assert(v[0] == 1);

    // Push causing reallocation
    v.push_back(2);
    assert(v.size() == 2);
    assert(v.capacity() == 2);
    assert(v[1] == 2);

    // Push without reallocation
    v.push_back(3);
    assert(v.size() == 3);
    assert(v.capacity() == 4);
    assert(v[2] == 3);

    // Push multiple elements
    for (int i = 4; i <= 10; ++i)
    {
        v.push_back(i);
    }
    assert(v.size() == 10);
    for (size_t i = 0; i < 10; ++i)
    {
        assert(v[i] == static_cast<int>(i + 1));
    }
}

// Test popBack
void test_pop_back()
{
    vector<int> v(5, 10);

    v.pop_back();
    assert(v.size() == 4);
    assert(v.capacity() == 5);

    v.pop_back();
    v.pop_back();
    assert(v.size() == 2);
}

// Test reserve
void test_reserve()
{
    vector<int> v;
    v.push_back(1);
    v.push_back(2);

    v.reserve(10);
    assert(v.size() == 2);
    assert(v.capacity() == 10);
    assert(v[0] == 1);
    assert(v[1] == 2);
}

// Test operator[]
void test_subscript_operator()
{
    vector<int> v(5);

    // Test writing
    for (size_t i = 0; i < v.size(); ++i)
    {
        v[i] = static_cast<int>(i * 2);
    }

    // Test reading
    for (size_t i = 0; i < v.size(); ++i)
    {
        assert(v[i] == static_cast<int>(i * 2));
    }

    // Test const version
    const vector<int> &cv = v;
    assert(cv[0] == 0);
    assert(cv[4] == 8);
}

// Test iterators
void test_iterators()
{
    vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);

    // Test mutable iterators
    int sum = 0;
    for (int *it = v.begin(); it != v.end(); ++it)
    {
        sum += *it;
    }
    assert(sum == 6);

    // Modify through iterator
    *v.begin() = 10;
    assert(v[0] == 10);

    // Test const iterators
    const vector<int> &cv = v;
    sum = 0;
    for (const int *it = cv.begin(); it != cv.end(); ++it)
    {
        sum += *it;
    }
    assert(sum == 15);
}

// Test with struct type
struct Point
{
    int x, y;
    Point() : x(0), y(0) {}
    Point(int x_, int y_) : x(x_), y(y_) {}
};

void test_with_structs()
{
    vector<Point> v;
    v.push_back(Point(1, 2));
    v.push_back(Point(3, 4));

    assert(v.size() == 2);
    assert(v[0].x == 1 && v[0].y == 2);
    assert(v[1].x == 3 && v[1].y == 4);

    vector<Point> v2(v);
    assert(v2[0].x == 1);
}

// Test edge cases
void test_edge_cases()
{
    // Empty vector operations
    vector<int> v;
    assert(v.size() == 0);
    assert(v.begin() == v.end());

    // Single element
    v.push_back(42);
    assert(v.size() == 1);
    assert(*v.begin() == 42);

    v.pop_back();
    assert(v.size() == 0);
}

// Test capacity growth
void test_capacity_growth()
{
    vector<int> v;

    size_t prev_capacity = 0;
    for (int i = 0; i < 100; ++i)
    {
        v.push_back(i);
        if (v.capacity() > prev_capacity)
        {
            // Verify capacity doubled (or started at 1)
            if (prev_capacity > 0)
            {
                assert(v.capacity() == prev_capacity * 2);
            }
            prev_capacity = v.capacity();
        }
    }

    assert(v.size() == 100);
    for (int i = 0; i < 100; ++i)
    {
        assert(v[i] == i);
    }
}

int main()
{
    test_default_constructor();
    test_resize_constructor();
    test_fill_constructor();
    test_copy_constructor();
    test_assignment_operator();
    test_move_constructor();
    test_move_assignment_operator();
    test_push_back();
    test_pop_back();
    test_reserve();
    test_subscript_operator();
    test_iterators();
    test_with_structs();
    test_edge_cases();
    test_capacity_growth();

    // If all tests pass, program exits normally
    return 0;
}
