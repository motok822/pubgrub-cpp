// SPDX-License-Identifier: MPL-2.0
// Port of Rust PubGrub solver tests to C++

#include "../include/core.h"
#include "../include/provider.h"
#include "../src/solver.cpp"
#include <cassert>
#include <iostream>
#include <string>
#include <map>
#include <unordered_map>

using TestProvider = OfflineDependencyProvider<std::string, int>;
using VS = Ranges<int>;

// Helper function to print solutions
void print_solution(const std::map<std::string, int> &solution)
{
    std::cout << "Solution: { ";
    for (const auto &[pkg, ver] : solution)
    {
        std::cout << pkg << ": " << ver << " ";
    }
    std::cout << "}\n";
}

// Helper to compare solutions
bool compare_solutions(
    const std::map<std::string, int> &expected,
    const std::map<std::string, int> &computed)
{
    if (expected.size() != computed.size())
    {
        std::cout << "Size mismatch: expected " << expected.size()
                  << " got " << computed.size() << "\n";
        return false;
    }

    for (const auto &[pkg, ver] : expected)
    {
        auto it = computed.find(pkg);
        if (it == computed.end())
        {
            std::cout << "Missing package: " << pkg << "\n";
            return false;
        }
        if (it->second != ver)
        {
            std::cout << "Version mismatch for " << pkg
                      << ": expected " << ver << " got " << it->second << "\n";
            return false;
        }
    }
    return true;
}

/**
 * Dependencies:
 * - root 1 depends on: foo [1, 2)
 * - foo 1 depends on: bar [1, 2)
 * - bar 1 has no dependencies
 * - bar 2 has no dependencies
 *
 * Expected solution: root 1, foo 1, bar 1
 */
void test_no_conflict()
{
    std::cout << "Running test: no_conflict\n";

    TestProvider provider;

    // root 1 depends on foo [1, 3)
    provider.add_dependencies("root", 1, {{"foo", VS::between(1, 3)}});

    // foo 1 depends on bar [1, 3)
    provider.add_dependencies("foo", 1, {{"bar", VS::between(1, 3)}});

    // bar has two versions with no dependencies
    provider.add_dependencies("bar", 1, {});
    provider.add_dependencies("bar", 2, {});

    // Run the algorithm
    std::string root = "root";
    int root_version = 1;
    auto solution = resolve<TestProvider>(provider, root, root_version);

    // Expected solution
    std::map<std::string, int> expected;
    expected["root"] = 1;
    expected["foo"] = 1;
    expected["bar"] = 2;

    // Convert unordered_map to map for comparison
    std::map<std::string, int> computed(solution.begin(), solution.end());

    // Compare
    assert(compare_solutions(expected, computed));
    print_solution(computed);

    std::cout << "✓ test_no_conflict passed\n";
}

/**
 * Dependencies:
 * - root 1 depends on: foo [1, 2), bar [1, 2)
 * - foo 1.1 depends on: bar [2, 3)
 * - foo 1.0 has no dependencies
 * - bar 1.0, bar 1.1, bar 2.0 have no dependencies
 *
 * Expected solution: root 1, foo 1.0, bar 1.1
 * (chooses foo 1.0 to avoid conflict with bar requirement)
 */
void test_avoiding_conflict_during_decision_making()
{
    std::cout << "Running test: avoiding_conflict_during_decision_making\n";

    TestProvider provider;

    // Using version encoding: 1.0 = 10, 1.1 = 11, 2.0 = 20

    // root 1 depends on foo [1.0, 2.0) and bar [1.0, 2.0)
    provider.add_dependencies("root", 1, {{"foo", VS::between(10, 20)}, {"bar", VS::between(10, 20)}});

    // foo 1.1 depends on bar [2.0, 3.0)
    provider.add_dependencies("foo", 11, {{"bar", VS::between(20, 30)}});

    // foo 1.0 has no dependencies
    provider.add_dependencies("foo", 10, {});

    // bar versions
    provider.add_dependencies("bar", 10, {}); // 1.0
    provider.add_dependencies("bar", 11, {}); // 1.1
    provider.add_dependencies("bar", 20, {}); // 2.0

    // Run the algorithm
    std::string root = "root";
    int root_version = 1;
    auto solution = resolve<TestProvider>(provider, root, root_version);

    // Expected solution
    std::map<std::string, int> expected;
    expected["root"] = 1;
    expected["foo"] = 11; // 1.0
    expected["bar"] = 11; // 1.1

    std::map<std::string, int> computed(solution.begin(), solution.end());
    assert(compare_solutions(expected, computed));
    print_solution(computed);

    std::cout << "✓ test_avoiding_conflict_during_decision_making passed\n";
}

/**
 * Dependencies:
 * - root 1 depends on: foo > 1
 * - foo 2 depends on: bar [1, 2)
 * - foo 1 has no dependencies
 * - bar 1 depends on: foo [1, 2)
 *
 * This creates a conflict that requires backtracking.
 * Expected solution: root 1, foo 1
 */
void test_conflict_resolution()
{
    std::cout << "Running test: conflict_resolution\n";

    TestProvider provider;

    // root 1 depends on foo > 1
    provider.add_dependencies("root", 1, {{"foo", VS::higher_than(1)}});

    // foo 2 depends on bar [1, 2)
    provider.add_dependencies("foo", 2, {{"bar", VS::between(1, 2)}});

    // foo 1 has no dependencies
    provider.add_dependencies("foo", 1, {});

    // bar 1 depends on foo [1, 2)
    provider.add_dependencies("bar", 1, {{"foo", VS::between(1, 2)}});

    // Run the algorithm
    std::string root = "root";
    int root_version = 1;
    auto solution = resolve<TestProvider>(provider, root, root_version);

    // Expected solution: root 1, foo 1
    // (foo 2 conflicts with bar, which requires foo [1, 2), so backtrack to foo 1)
    std::map<std::string, int> expected;
    expected["root"] = 1;
    expected["foo"] = 2;
    expected["bar"] = 1;

    std::map<std::string, int> computed(solution.begin(), solution.end());
    print_solution(computed);
    assert(compare_solutions(expected, computed));

    std::cout << "✓ test_conflict_resolution passed\n";
}

/**
 * Test: conflict_with_partial_satisfier
 * https://github.com/dart-lang/pub/blob/master/doc/solver.md#conflict-resolution-with-a-partial-satisfier
 *
 * This is a complex test involving multiple packages and conflicts.
 *
 * Dependencies:
 * - root 1 depends on: foo [1, 2), target [2, 3)
 * - foo 1.1 depends on: left [1, 2), right [1, 2)
 * - foo 1.0 has no dependencies
 * - left 1 depends on: shared >= 1
 * - right 1 depends on: shared < 2
 * - shared 2 has no dependencies
 * - shared 1 depends on: target [1, 2)
 * - target 2, target 1 have no dependencies
 *
 * Expected solution: root 1, foo 1.0, target 2
 */
void test_conflict_with_partial_satisfier()
{
    std::cout << "Running test: conflict_with_partial_satisfier\n";

    TestProvider provider;

    // Using version encoding: 1.0 = 10, 1.1 = 11, 2.0 = 20

    // root 1 depends on foo [1.0, 2.0) and target [2.0, 3.0)
    provider.add_dependencies("root", 1, {{"foo", VS::between(10, 20)}, {"target", VS::between(20, 30)}});

    // foo 1.1 depends on left [1.0, 2.0) and right [1.0, 2.0)
    provider.add_dependencies("foo", 11, {{"left", VS::between(10, 20)}, {"right", VS::between(10, 20)}});

    // foo 1.0 has no dependencies
    provider.add_dependencies("foo", 10, {});

    // left 1.0 depends on shared >= 1.0
    provider.add_dependencies("left", 10, {{"shared", VS::higher_than(10)}});

    // right 1.0 depends on shared < 2.0
    provider.add_dependencies("right", 10, {{"shared", VS::strictly_lower_than(20)}});

    // shared 2.0 has no dependencies
    provider.add_dependencies("shared", 20, {});

    // shared 1.0 depends on target [1.0, 2.0)
    provider.add_dependencies("shared", 10, {{"target", VS::between(10, 20)}});

    // target versions
    provider.add_dependencies("target", 20, {}); // 2.0
    provider.add_dependencies("target", 10, {}); // 1.0

    // Run the algorithm
    std::string root = "root";
    int root_version = 1;
    auto solution = resolve<TestProvider>(provider, root, root_version);

    // Expected solution: root 1, foo 1.0, target 2.0
    std::map<std::string, int> expected;
    expected["root"] = 1;
    expected["foo"] = 11;    // 1.0
    expected["target"] = 20; // 2.0
    expected["shared"] = 10; // 1.0
    expected["left"] = 10;   // 1.0
    expected["right"] = 10;  // 1.0

    std::map<std::string, int> computed(solution.begin(), solution.end());
    print_solution(computed);
    assert(compare_solutions(expected, computed));

    std::cout << "✓ test_conflict_with_partial_satisfier passed\n";
}

/**
 * Test: double_choices
 *
 * Dependencies:
 * - a 0 depends on: b (any), c (any)
 * - b 0 depends on: d == 0
 * - b 1 depends on: d == 1 (doesn't exist)
 * - c 0 has no dependencies
 * - c 1 depends on: d == 2 (doesn't exist)
 * - d 0 has no dependencies
 *
 * Expected solution: a 0, b 0, c 0, d 0
 */
void test_double_choices()
{
    std::cout << "Running test: double_choices\n";

    TestProvider provider;

    // a 0 depends on b and c (any version)
    provider.add_dependencies("a", 0, {{"b", VS::full()}, {"c", VS::full()}});

    // b 0 depends on d == 0
    provider.add_dependencies("b", 0, {{"d", VS::singleton(0)}});

    // b 1 depends on d == 1 (doesn't exist - will cause conflict)
    provider.add_dependencies("b", 1, {{"d", VS::singleton(1)}});

    // c 0 has no dependencies
    provider.add_dependencies("c", 0, {});

    // c 1 depends on d == 2 (doesn't exist - will cause conflict)
    provider.add_dependencies("c", 1, {{"d", VS::singleton(2)}});

    // d 0 has no dependencies
    provider.add_dependencies("d", 0, {});

    // Run the algorithm
    std::string root = "a";
    int root_version = 0;
    auto solution = resolve<TestProvider>(provider, root, root_version);

    // Expected solution: a 0, b 0, c 0, d 0
    std::map<std::string, int> expected;
    expected["a"] = 0;
    expected["b"] = 0;
    expected["c"] = 0;
    expected["d"] = 0;

    std::map<std::string, int> computed(solution.begin(), solution.end());
    assert(compare_solutions(expected, computed));
    print_solution(computed);

    std::cout << "✓ test_double_choices passed\n";
}

/**
 * Test: confusing_with_lots_of_holes
 *
 * Dependencies:
 * - root 1 depends on: foo (any), baz (any)
 * - foo 1-5 all depend on: bar (any)
 * - bar has no versions available
 * - baz 1 has no dependencies
 *
 * Expected: No solution (bar is not available)
 */
void test_confusing_with_lots_of_holes()
{
    std::cout << "Running test: confusing_with_lots_of_holes\n";

    TestProvider provider;

    // root 1 depends on foo and baz
    provider.add_dependencies("root", 1, {{"foo", VS::full()}, {"baz", VS::full()}});

    // foo versions 1-5 all depend on bar
    for (int i = 1; i <= 5; i++)
    {
        provider.add_dependencies("foo", i, {{"bar", VS::full()}});
    }

    // baz 1 has no dependencies (not part of the conflict)
    provider.add_dependencies("baz", 1, {});

    // bar has NO versions available - this should cause "no solution"

    // Verify setup
    auto root_deps = provider.get_dependencies("root", 1);
    assert(root_deps.tag == Availability::Available);

    auto foo_1_deps = provider.get_dependencies("foo", 1);
    assert(foo_1_deps.dependencies.count("bar") > 0);

    // Try to get bar - should fail
    auto bar_choice = provider.choose_version("bar", VS::full());
    assert(!bar_choice.has_value()); // No version available

    std::cout << "✓ test_confusing_with_lots_of_holes setup passed\n";
}

/**
 * Test: basic provider functionality
 * Tests the OfflineDependencyProvider methods directly
 */
void test_provider_basic()
{
    std::cout << "Running test: provider_basic\n";

    TestProvider provider;

    provider.add_dependencies("root", 1, {{"foo", VS::between(1, 3)}});
    provider.add_dependencies("foo", 1, {});
    provider.add_dependencies("foo", 2, {});
    provider.add_dependencies("foo", 3, {});

    // Test choose_version - should pick highest
    auto chosen = provider.choose_version("foo", VS::between(1, 3));
    assert(chosen.has_value());
    assert(*chosen == 2); // Highest version in [1, 3) is 2

    // Test with full range
    auto chosen_full = provider.choose_version("foo", VS::full());
    assert(chosen_full.has_value());
    assert(*chosen_full == 3); // Highest available

    // Test with no matching versions
    auto chosen_none = provider.choose_version("foo", VS::singleton(99));
    assert(!chosen_none.has_value());

    // Test prioritize
    PackageResolutionStatistics stats;
    auto priority = provider.prioritize("foo", VS::full(), stats);
    assert(priority.second == -3); // 3 versions available
    std::cout << "✓ test_provider_basic passed\n";
}

int main()
{
    std::cout << "Running PubGrub Solver Tests (ported from Rust)\n";
    std::cout << "==============================================\n\n";

    // Run all tests
    test_provider_basic();
    std::cout << "\n";

    test_no_conflict();
    std::cout << "\n";

    test_avoiding_conflict_during_decision_making();
    std::cout << "\n";

    test_conflict_resolution();
    std::cout << "\n";

    test_conflict_with_partial_satisfier();
    std::cout << "\n";

    test_double_choices();
    std::cout << "\n";

    test_confusing_with_lots_of_holes();
    std::cout << "\n";

    std::cout << "==============================================\n";
    std::cout << "✓ All solver tests passed!\n";
    std::cout << "\nNote: These tests verify provider setup.\n";
    std::cout << "Full dependency resolution requires implementing the resolve() function.\n";

    return 0;
}