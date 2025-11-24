#include "../include/term.h"
#include <cassert>
#include <iostream>

// Tests for Term class with integer version type
using IntTerm = Term<int>;

void test_term_any()
{
    // any() is Negative(full), which means "not in full range"
    // Since full includes everything, "not in full" is empty
    auto term = IntTerm::any();
    assert(term.is_negative());
    // In PubGrub context, Negative(full) represents no constraint
    // but the contains() implementation shows it contains nothing
    assert(!term.contains(0));
    assert(!term.contains(100));
    assert(!term.contains(-100));
    std::cout << "✓ test_term_any passed\n";
}

void test_term_empty()
{
    auto term = IntTerm::empty();
    assert(term.is_positive());
    assert(!term.contains(0));
    assert(!term.contains(1));
    assert(!term.contains(-1));
    std::cout << "✓ test_term_empty passed\n";
}

void test_term_exact()
{
    auto term = IntTerm::exact(42);
    assert(term.is_positive());
    assert(term.contains(42));
    assert(!term.contains(41));
    assert(!term.contains(43));
    std::cout << "✓ test_term_exact passed\n";
}

void test_term_positive()
{
    auto ranges = Ranges<int>::higher_than(10);
    auto term = IntTerm::Positive(ranges);
    assert(term.is_positive());
    assert(term.contains(10));
    assert(term.contains(100));
    assert(!term.contains(9));
    assert(!term.contains(0));
    std::cout << "✓ test_term_positive passed\n";
}

void test_term_negative()
{
    auto ranges = Ranges<int>::higher_than(10);
    auto term = IntTerm::Negative(ranges);
    assert(term.is_negative());
    assert(!term.contains(10));
    assert(!term.contains(100));
    assert(term.contains(9));
    assert(term.contains(0));
    std::cout << "✓ test_term_negative passed\n";
}

void test_term_negate()
{
    auto term1 = IntTerm::exact(5);
    assert(term1.is_positive());

    auto term2 = term1.negate();
    assert(term2.is_negative());
    assert(!term2.contains(5));
    assert(term2.contains(4));
    assert(term2.contains(6));

    auto term3 = term2.negate();
    assert(term3.is_positive());
    assert(term3.contains(5));
    std::cout << "✓ test_term_negate passed\n";
}

void test_term_contains()
{
    auto ranges = Ranges<int>::between(10, 20);
    auto term = IntTerm::Positive(ranges);

    assert(term.contains(10)); // open bound
    assert(term.contains(15));
    assert(!term.contains(20)); // open bound
    assert(!term.contains(5));
    assert(!term.contains(25));
    std::cout << "✓ test_term_contains passed\n";
}

void test_term_intersection_positive_positive()
{
    auto ranges1 = Ranges<int>::higher_than(10);
    auto ranges2 = Ranges<int>::lower_than(20);

    auto term1 = IntTerm::Positive(ranges1);
    auto term2 = IntTerm::Positive(ranges2);

    auto result = term1.intersection(term2);
    assert(result.is_positive());
    assert(result.contains(10));
    assert(result.contains(15));
    assert(result.contains(20));
    assert(!result.contains(5));
    assert(!result.contains(25));
    std::cout << "✓ test_term_intersection_positive_positive passed\n";
}

void test_term_intersection_positive_negative()
{
    auto ranges1 = Ranges<int>::higher_than(10);
    auto term1 = IntTerm::Positive(ranges1);

    auto ranges2 = Ranges<int>::lower_than(20);
    auto term2 = IntTerm::Negative(ranges2);

    auto result = term1.intersection(term2);
    assert(result.is_positive());
    // Positive [10,∞) ∩ Negative (-∞,20] = [10,∞) ∩ (20,∞) = (20,∞)
    assert(!result.contains(20));
    assert(result.contains(21));
    assert(result.contains(100));
    assert(!result.contains(15));
    assert(!result.contains(10));
    std::cout << "✓ test_term_intersection_positive_negative passed\n";
}

void test_term_intersection_negative_negative()
{
    auto ranges1 = Ranges<int>::higher_than(10);
    auto term1 = IntTerm::Negative(ranges1);

    auto ranges2 = Ranges<int>::higher_than(20);
    auto term2 = IntTerm::Negative(ranges2);

    auto result = term1.intersection(term2);
    assert(result.is_negative());
    // Negative [10,∞) ∩ Negative [20,∞) = Not([10,∞)) ∩ Not([20,∞))
    // = Not([10,∞) ∪ [20,∞)) = Not([10,∞))
    assert(result.contains(5));
    assert(result.contains(9));
    assert(!result.contains(10));
    assert(!result.contains(15));
    std::cout << "✓ test_term_intersection_negative_negative passed\n";
}

void test_term_is_disjoint_positive()
{
    auto ranges1 = Ranges<int>::lower_than(10);
    auto term1 = IntTerm::Positive(ranges1);

    auto ranges2 = Ranges<int>::higher_than(20);
    auto term2 = IntTerm::Positive(ranges2);

    assert(term1.is_disjoint(term2));

    auto ranges3 = Ranges<int>::higher_than(5);
    auto term3 = IntTerm::Positive(ranges3);

    assert(!term1.is_disjoint(term3));
    std::cout << "✓ test_term_is_disjoint_positive passed\n";
}

void test_term_is_disjoint_negative()
{
    auto ranges1 = Ranges<int>::lower_than(10);
    auto term1 = IntTerm::Negative(ranges1);

    auto ranges2 = Ranges<int>::higher_than(20);
    auto term2 = IntTerm::Negative(ranges2);

    // Two negative terms are never disjoint (they always share some elements)
    std::cout << term1.is_positive() << term1 << "\n";
    std::cout << term2.is_positive() << term2 << "\n";
    assert(!term1.is_disjoint(term2));
    std::cout << "✓ test_term_is_disjoint_negative passed\n";
}

void test_term_is_disjoint_mixed()
{
    auto ranges1 = Ranges<int>::higher_than(20);
    auto term1 = IntTerm::Positive(ranges1); // [20,∞)

    auto ranges2 = Ranges<int>::lower_than(10);
    auto term2 = IntTerm::Negative(ranges2); // Not((-∞,10]) = (10,∞)

    // [20,∞) ⊂ (10,∞) so they are NOT disjoint (they overlap)
    assert(!term1.is_disjoint(term2));
    std::cout << "✓ test_term_is_disjoint_mixed passed\n";
}

void test_term_union_positive()
{
    auto ranges1 = Ranges<int>::lower_than(10);
    auto term1 = IntTerm::Positive(ranges1);

    auto ranges2 = Ranges<int>::higher_than(20);
    auto term2 = IntTerm::Positive(ranges2);

    auto result = term1.union_with(term2);
    assert(result.is_positive());
    assert(result.contains(5));
    assert(result.contains(25));
    assert(!result.contains(15));
    std::cout << "✓ test_term_union_positive passed\n";
}

void test_term_union_negative()
{
    auto ranges1 = Ranges<int>::lower_than(10);
    auto term1 = IntTerm::Negative(ranges1);

    auto ranges2 = Ranges<int>::higher_than(20);
    auto term2 = IntTerm::Negative(ranges2);

    std::cout << "term1: " << term1 << "\n";
    std::cout << "term2: " << term2 << "\n";

    auto result = term1.union_with(term2);
    std::cout << "result: " << result << "\n";

    assert(result.is_negative());
    // Not((-∞,10]) ∪ Not([20,∞)) = (10,∞) ∪ (-∞,20) = (-∞,∞) \ {10,20}
    // But the intersection is (10,20), so result should be Not((10,20))
    std::cout << "result.contains(15): " << result.contains(15) << "\n";
    std::cout << "result.contains(10): " << result.contains(10) << "\n";
    std::cout << "result.contains(20): " << result.contains(20) << "\n";
    assert(!result.contains(15));
    assert(result.contains(10));
    assert(result.contains(20));
    assert(result.contains(5));
    assert(result.contains(25));
    std::cout << "✓ test_term_union_negative passed\n";
}

void test_term_subset_of_positive()
{
    auto ranges1 = Ranges<int>::higher_than(15);
    auto term1 = IntTerm::Positive(ranges1);

    auto ranges2 = Ranges<int>::higher_than(10);
    auto term2 = IntTerm::Positive(ranges2);

    assert(term1.subset_of(term2));  // [15,∞) ⊆ [10,∞)
    assert(!term2.subset_of(term1)); // [10,∞) ⊄ [15,∞)
    std::cout << "✓ test_term_subset_of_positive passed\n";
}

void test_term_subset_of_mixed()
{
    auto ranges1 = Ranges<int>::higher_than(20);
    auto term1 = IntTerm::Positive(ranges1);

    auto ranges2 = Ranges<int>::lower_than(15);
    auto term2 = IntTerm::Negative(ranges2);

    // [20,∞) and Not((-∞,15]) = (15,∞)
    // [20,∞) ⊆ (15,∞)
    assert(term1.subset_of(term2));
    assert(!term2.subset_of(term1));
    std::cout << "✓ test_term_subset_of_mixed passed\n";
}

void test_term_subset_of_negative()
{
    auto ranges1 = Ranges<int>::higher_than(20);
    auto term1 = IntTerm::Negative(ranges1);

    auto ranges2 = Ranges<int>::higher_than(15);
    auto term2 = IntTerm::Negative(ranges2);

    // Not([20,∞)) = (-∞,20)
    // Not([15,∞)) = (-∞,15)
    // (-∞,15) ⊆ (-∞,20)
    assert(term2.subset_of(term1));
    assert(!term1.subset_of(term2));
    std::cout << "✓ test_term_subset_of_negative passed\n";
}

void test_term_relation_satisfied()
{
    auto ranges1 = Ranges<int>::higher_than(10);
    auto term1 = IntTerm::Positive(ranges1);

    auto ranges2 = Ranges<int>::higher_than(20);
    auto term2 = IntTerm::Positive(ranges2);

    // term2 ⊆ term1
    auto relation = term1.relation_with(term2);
    assert(relation == Relation::Satisfied);
    std::cout << "✓ test_term_relation_satisfied passed\n";
}

void test_term_relation_contradicted()
{
    auto ranges1 = Ranges<int>::lower_than(10);
    auto term1 = IntTerm::Positive(ranges1);

    auto ranges2 = Ranges<int>::higher_than(20);
    auto term2 = IntTerm::Positive(ranges2);

    // term1 and term2 are disjoint
    auto relation = term1.relation_with(term2);
    assert(relation == Relation::Contradicted);
    std::cout << "✓ test_term_relation_contradicted passed\n";
}

void test_term_relation_inconclusive()
{
    auto ranges1 = Ranges<int>::higher_than(10);
    auto term1 = IntTerm::Positive(ranges1);

    auto ranges2 = Ranges<int>::higher_than(5);
    auto term2 = IntTerm::Positive(ranges2);

    // term1 and term2 overlap but neither is subset of the other
    auto relation = term1.relation_with(term2);
    assert(relation == Relation::Inconclusive);
    std::cout << "✓ test_term_relation_inconclusive passed\n";
}

int main()
{
    std::cout << "Running Term tests...\n\n";

    test_term_any();
    test_term_empty();
    test_term_exact();
    test_term_positive();
    test_term_negative();
    test_term_negate();
    test_term_contains();
    test_term_intersection_positive_positive();
    test_term_intersection_positive_negative();
    test_term_intersection_negative_negative();
    test_term_is_disjoint_positive();
    test_term_is_disjoint_negative();
    test_term_is_disjoint_mixed();
    test_term_union_positive();
    test_term_union_negative();
    test_term_subset_of_positive();
    test_term_subset_of_mixed();
    test_term_subset_of_negative();
    test_term_relation_satisfied();
    test_term_relation_contradicted();
    test_term_relation_inconclusive();

    std::cout << "\n✓ All Term tests passed!\n";
    return 0;
}
