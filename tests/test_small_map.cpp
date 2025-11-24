#include "../include/small_map.h"
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>

void test_small_map_empty()
{
    SmallMap<int, std::string> map;
    assert(map.len() == 0);
    const auto *val = map.get(1);
    assert(val == nullptr);
    std::cout << "✓ test_small_map_empty passed\n";
}

void test_small_map_one()
{
    SmallMap<int, std::string> map;
    map.insert(1, "one");

    assert(map.len() == 1);
    const auto *val = map.get(1);
    assert(val != nullptr);
    assert(*val == "one");

    const auto *missing = map.get(2);
    assert(missing == nullptr);

    std::cout << "✓ test_small_map_one passed\n";
}

void test_small_map_two()
{
    SmallMap<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");

    assert(map.len() == 2);
    assert(*map.get(1) == "one");
    assert(*map.get(2) == "two");

    std::cout << "✓ test_small_map_two passed\n";
}

void test_small_map_three_plus()
{
    SmallMap<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");
    map.insert(4, "four");
    map.insert(5, "five");

    assert(map.len() == 5);
    assert(*map.get(1) == "one");
    assert(*map.get(2) == "two");
    assert(*map.get(3) == "three");
    assert(*map.get(4) == "four");
    assert(*map.get(5) == "five");

    std::cout << "✓ test_small_map_three_plus passed\n";
}

void test_small_map_update()
{
    SmallMap<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");

    assert(*map.get(1) == "one");

    // Update existing key
    map.insert(1, "ONE");
    assert(*map.get(1) == "ONE");
    assert(map.len() == 2); // Size should not change

    std::cout << "✓ test_small_map_update passed\n";
}

void test_small_map_remove()
{
    SmallMap<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");

    assert(map.len() == 3);

    map.remove(2);
    assert(map.len() == 2);
    assert(*map.get(1) == "one");
    assert(map.get(2) == nullptr);
    assert(*map.get(3) == "three");

    map.remove(1);
    assert(map.len() == 1);
    assert(map.get(1) == nullptr);
    assert(*map.get(3) == "three");

    map.remove(3);
    assert(map.len() == 0);

    std::cout << "✓ test_small_map_remove passed\n";
}

void test_small_map_iterator_small()
{
    SmallMap<int, std::string> map;
    map.insert(10, "ten");
    map.insert(20, "twenty");

    // Test range-based for loop (only works for small sizes)
    int count = 0;
    for (const auto &pair : map)
    {
        count++;
        assert(pair.first == 10 || pair.first == 20);
        assert(pair.second == "ten" || pair.second == "twenty");
    }
    assert(count == 2);

    std::cout << "✓ test_small_map_iterator_small passed\n";
}

void test_small_map_iterator_large()
{
    SmallMap<int, std::string> map;
    for (int i = 1; i <= 5; ++i)
    {
        map.insert(i, "number_" + std::to_string(i));
    }

    // Test range-based for loop
    int count = 0;
    for (const auto &pair : map)
    {
        count++;
        assert(pair.first >= 1 && pair.first <= 5);
        assert(pair.second == "number_" + std::to_string(pair.first));
    }
    assert(count == 5);

    std::cout << "✓ test_small_map_iterator_large passed\n";
}

void test_small_map_string_keys()
{
    SmallMap<std::string, int> map;
    map.insert("one", 1);
    map.insert("two", 2);
    map.insert("three", 3);
    map.insert("four", 4);

    assert(map.len() == 4);
    assert(*map.get("one") == 1);
    assert(*map.get("two") == 2);
    assert(*map.get("three") == 3);
    assert(*map.get("four") == 4);

    std::cout << "✓ test_small_map_string_keys passed\n";
}

int main()
{
    std::cout << "Running SmallMap tests...\n\n";

    test_small_map_empty();
    test_small_map_one();
    test_small_map_two();
    test_small_map_three_plus();
    test_small_map_update();
    test_small_map_remove();
    test_small_map_iterator_small();
    test_small_map_string_keys();

    std::cout << "\n✓ All SmallMap tests passed!\n";
    return 0;
}
