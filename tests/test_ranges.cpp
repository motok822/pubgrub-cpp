#include "../include/ranges.h"
#include <cassert>
#include <iostream>

void test_ranges_empty()
{
    auto r = Ranges<int>::empty();
    assert(r.is_empty());
    std::cout << "✓ test_ranges_empty passed\n";
}

void test_ranges_full()
{
    auto r = Ranges<int>::full();
    assert(!r.is_empty());
    assert(r.contains(0));
    assert(r.contains(100));
    assert(r.contains(-100));
    std::cout << "✓ test_ranges_full passed\n";
}

void test_ranges_singleton()
{
    auto r = Ranges<int>::singleton(5);
    assert(r.contains(5));
    assert(!r.contains(4));
    assert(!r.contains(6));

    auto singleton_val = r.as_singleton();
    assert(singleton_val.has_value());
    assert(*singleton_val == 5);
    std::cout << "✓ test_ranges_singleton passed\n";
}

void test_ranges_higher_than()
{
    auto r = Ranges<int>::higher_than(5);
    assert(r.contains(5));
    assert(r.contains(6));
    assert(r.contains(100));
    assert(!r.contains(4));
    std::cout << "✓ test_ranges_higher_than passed\n";
}

void test_ranges_strictly_higher_than()
{
    auto r = Ranges<int>::strictly_higher_than(5);
    assert(!r.contains(5));
    assert(r.contains(6));
    assert(r.contains(100));
    assert(!r.contains(4));
    std::cout << "✓ test_ranges_strictly_higher_than passed\n";
}

void test_ranges_lower_than()
{
    auto r = Ranges<int>::lower_than(5);
    assert(r.contains(5));
    assert(r.contains(4));
    assert(r.contains(-100));
    assert(!r.contains(6));
    std::cout << "✓ test_ranges_lower_than passed\n";
}

void test_ranges_strictly_lower_than()
{
    auto r = Ranges<int>::strictly_lower_than(5);
    assert(!r.contains(5));
    assert(r.contains(4));
    assert(r.contains(-100));
    assert(!r.contains(6));
    std::cout << "✓ test_ranges_strictly_lower_than passed\n";
}

void test_ranges_between()
{
    auto r = Ranges<int>::between(5, 10);
    assert(r.contains(5));
    assert(r.contains(6));
    assert(r.contains(9));
    assert(!r.contains(10));
    assert(!r.contains(4));
    assert(!r.contains(11));
    std::cout << "✓ test_ranges_between passed\n";
}

void test_ranges_complement()
{
    auto r = Ranges<int>::higher_than(5);
    auto comp = r.complement();
    assert(!comp.contains(5));
    assert(!comp.contains(6));
    assert(comp.contains(4));
    std::cout << "✓ test_ranges_complement passed\n";
}

void test_ranges_union()
{
    auto r1 = Ranges<int>::higher_than(10);
    auto r2 = Ranges<int>::lower_than(5);
    auto u = r1.union_(r2);

    assert(u.contains(0));
    assert(u.contains(5));
    assert(!u.contains(7));
    assert(u.contains(10));
    assert(u.contains(15));
    std::cout << "✓ test_ranges_union passed\n";
}

void test_ranges_intersection()
{
    auto r1 = Ranges<int>::higher_than(5);
    auto r2 = Ranges<int>::lower_than(10);
    auto inter = r1.intersection(r2);

    assert(!inter.contains(4));
    assert(inter.contains(5));
    assert(inter.contains(7));
    assert(inter.contains(10));
    assert(!inter.contains(11));
    std::cout << "✓ test_ranges_intersection passed\n";
}

void test_ranges_is_disjoint()
{
    auto r1 = Ranges<int>::higher_than(10);
    auto r2 = Ranges<int>::lower_than(5);
    assert(r1.is_disjoint(r2));

    auto r3 = Ranges<int>::higher_than(5);
    auto r4 = Ranges<int>::lower_than(10);
    assert(!r3.is_disjoint(r4));
    std::cout << "✓ test_ranges_is_disjoint passed\n";
}

void test_ranges_subset_of()
{
    auto r1 = Ranges<int>::higher_than(10);
    auto r2 = Ranges<int>::higher_than(5);
    assert(r1.subset_of(r2));
    assert(!r2.subset_of(r1));
    std::cout << "✓ test_ranges_subset_of passed\n";
}

void test_ranges_equality()
{
    auto r1 = Ranges<int>::singleton(5);
    auto r2 = Ranges<int>::singleton(5);
    auto r3 = Ranges<int>::singleton(6);

    assert(r1 == r2);
    assert(r1 != r3);
    std::cout << "✓ test_ranges_equality passed\n";
}

int main()
{
    std::cout << "Running Ranges tests...\n\n";

    test_ranges_empty();
    test_ranges_full();
    test_ranges_singleton();
    test_ranges_higher_than();
    test_ranges_strictly_higher_than();
    test_ranges_lower_than();
    test_ranges_strictly_lower_than();
    test_ranges_between();
    test_ranges_complement();
    test_ranges_union();
    test_ranges_intersection();
    test_ranges_is_disjoint();
    test_ranges_subset_of();
    test_ranges_equality();

    std::cout << "\n✓ All Ranges tests passed!\n";
    return 0;
}
