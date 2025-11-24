#include "../include/small_map.h"
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>

void test_small_vec_empty()
{
    SmallVec<int> vec;
    assert(vec.size() == 0);
    assert(vec.empty_vec() == true);
    assert(vec.begin() == vec.end());
    std::cout << "✓ test_small_vec_empty passed\n";
}

void test_small_vec_one()
{
    SmallVec<int> vec;
    vec.push(42);
    assert(vec.size() == 1);
    assert(vec.empty_vec() == false);
    assert(vec[0] == 42);
    assert(*vec.begin() == 42);
    assert(vec.end() - vec.begin() == 1);
    std::cout << "✓ test_small_vec_one passed\n";
}

void test_small_vec_two()
{
    SmallVec<int> vec;
    vec.push(10);
    vec.push(20);
    assert(vec.size() == 2);
    assert(vec[0] == 10);
    assert(vec[1] == 20);
    std::cout << "✓ test_small_vec_two passed\n";
}

void test_small_vec_three_plus()
{
    SmallVec<int> vec;
    vec.push(1);
    vec.push(2);
    vec.push(3);
    vec.push(4);
    vec.push(5);

    assert(vec.size() == 5);
    assert(vec[0] == 1);
    assert(vec[1] == 2);
    assert(vec[2] == 3);
    assert(vec[3] == 4);
    assert(vec[4] == 5);
    std::cout << "✓ test_small_vec_three_plus passed\n";
}

void test_small_vec_iterator()
{
    SmallVec<int> vec;
    vec.push(10);
    vec.push(20);
    vec.push(30);

    // Test range-based for loop
    int sum = 0;
    for (const auto &val : vec)
    {
        sum += val;
    }
    assert(sum == 60);

    // Test iterator arithmetic
    auto it = vec.begin();
    assert(*it == 10);
    ++it;
    assert(*it == 20);
    ++it;
    assert(*it == 30);
    ++it;
    assert(it == vec.end());

    std::cout << "✓ test_small_vec_iterator passed\n";
}

void test_small_vec_const_iterator()
{
    SmallVec<int> vec;
    vec.push(5);
    vec.push(15);
    vec.push(25);

    const SmallVec<int> &cvec = vec;

    int count = 0;
    for (auto it = cvec.begin(); it != cvec.end(); ++it)
    {
        count++;
    }
    assert(count == 3);

    std::cout << "✓ test_small_vec_const_iterator passed\n";
}

void test_small_vec_view()
{
    SmallVec<int> vec;
    vec.push(100);
    vec.push(200);

    auto v = vec.view();
    assert(v.size() == 2);
    assert(v[0] == 100);
    assert(v[1] == 200);

    // Test const view
    const SmallVec<int> &cvec = vec;
    auto cv = cvec.view();
    assert(cv.size() == 2);
    assert(cv[0] == 100);

    std::cout << "✓ test_small_vec_view passed\n";
}

void test_small_vec_data()
{
    SmallVec<int> vec;
    vec.push(7);
    vec.push(8);
    vec.push(9);

    int *ptr = vec.data();
    assert(ptr[0] == 7);
    assert(ptr[1] == 8);
    assert(ptr[2] == 9);

    std::cout << "✓ test_small_vec_data passed\n";
}

void test_small_vec_clear()
{
    SmallVec<int> vec;
    vec.push(1);
    vec.push(2);
    vec.push(3);
    assert(vec.size() == 3);

    vec.clear();
    assert(vec.size() == 0);
    assert(vec.empty_vec() == true);

    std::cout << "✓ test_small_vec_clear passed\n";
}

void test_small_vec_with_strings()
{
    SmallVec<std::string> vec;
    vec.push("hello");
    vec.push("world");
    vec.push("test");

    assert(vec.size() == 3);
    assert(vec[0] == "hello");
    assert(vec[1] == "world");
    assert(vec[2] == "test");

    // Test iteration
    std::string result;
    for (const auto &s : vec)
    {
        result += s;
    }
    assert(result == "helloworldtest");

    std::cout << "✓ test_small_vec_with_strings passed\n";
}

void test_small_vec_stl_algorithms()
{
    SmallVec<int> vec;
    vec.push(3);
    vec.push(1);
    vec.push(4);
    vec.push(1);
    vec.push(5);

    // Test std::find
    auto it = std::find(vec.begin(), vec.end(), 4);
    assert(it != vec.end());
    assert(*it == 4);

    // Test std::count
    int count = std::count(vec.begin(), vec.end(), 1);
    assert(count == 2);

    // Test std::sort
    std::sort(vec.begin(), vec.end());
    assert(vec[0] == 1);
    assert(vec[1] == 1);
    assert(vec[2] == 3);
    assert(vec[3] == 4);
    assert(vec[4] == 5);

    std::cout << "✓ test_small_vec_stl_algorithms passed\n";
}

int main()
{
    std::cout << "Running SmallVec tests...\n\n";

    test_small_vec_empty();
    test_small_vec_one();
    test_small_vec_two();
    test_small_vec_three_plus();
    test_small_vec_iterator();
    test_small_vec_const_iterator();
    test_small_vec_view();
    test_small_vec_data();
    test_small_vec_clear();
    test_small_vec_with_strings();
    test_small_vec_stl_algorithms();

    std::cout << "\n✓ All SmallVec tests passed!\n";
    return 0;
}
