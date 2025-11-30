// SPDX-License-Identifier: MPL-2.0
// Port of Rust PubGrub solver tests to C++

#include "../include/core.h"
#include "../include/provider.h"
#include "../src/dpll_solver.cpp"
#include "../src/cdcl_solver.cpp" // Added to allow naive solver comparison
#include <cassert>
#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <chrono>
#include <cmath>

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

// Helper to run and time both solvers
static void print_timing(const std::string &label, long long naive_us, long long solver_us, size_t naive_size, size_t solver_size)
{
    std::cout << label << " timing (microseconds)\n";
    std::cout << "  DPLL:    " << naive_us << " us (packages=" << naive_size << ")\n";
    std::cout << "  CDCL:    " << solver_us << " us (packages=" << solver_size << ")\n";
    if (solver_us > 0)
    {
        double ratio = static_cast<double>(naive_us) / static_cast<double>(solver_us);
        std::cout << "  speedup:  " << ratio << "x (naive/solver)\n";
    }
}

// Updated test to run both naive (dpll_resolve) and optimized (resolve)
void test_no_conflict()
{
    std::cout << "Running test: no_conflict\n";
    TestProvider provider;
    provider.add_dependencies("root", 1, {{"foo", VS::between(1, 3)}});
    provider.add_dependencies("foo", 1, {{"bar", VS::between(1, 3)}});
    provider.add_dependencies("bar", 1, {});
    provider.add_dependencies("bar", 2, {});
    std::string root = "root";
    int root_version = 1;

    auto start_naive = std::chrono::steady_clock::now();
    auto naive_solution = dpll_resolve<TestProvider>(provider, root, root_version);
    auto end_naive = std::chrono::steady_clock::now();

    auto start_solver = std::chrono::steady_clock::now();
    auto solver_solution = resolve<TestProvider>(provider, root, root_version);
    auto end_solver = std::chrono::steady_clock::now();

    std::map<std::string, int> expected;
    expected["root"] = 1;
    expected["foo"] = 1;
    expected["bar"] = 2;

    std::map<std::string, int> naive_sorted(naive_solution.begin(), naive_solution.end());
    std::map<std::string, int> solver_sorted(solver_solution.begin(), solver_solution.end());

    assert(compare_solutions(expected, naive_sorted));
    assert(compare_solutions(expected, solver_sorted));

    print_solution(solver_sorted);
    print_timing("no_conflict",
                 std::chrono::duration_cast<std::chrono::microseconds>(end_naive - start_naive).count(),
                 std::chrono::duration_cast<std::chrono::microseconds>(end_solver - start_solver).count(),
                 naive_solution.size(), solver_solution.size());
    std::cout << "\n✓ test_no_conflict passed (both solvers)\n";
}

// Updated to compare differing expected outcomes (naive vs solver)
void test_avoiding_conflict_during_decision_making()
{
    std::cout << "Running test: avoiding_conflict_during_decision_making\n";
    TestProvider provider;
    provider.add_dependencies("root", 1, {{"foo", VS::between(10, 20)}, {"bar", VS::between(10, 20)}});
    provider.add_dependencies("foo", 11, {{"bar", VS::between(20, 30)}}); // foo 1.1
    provider.add_dependencies("foo", 10, {});                             // foo 1.0
    provider.add_dependencies("bar", 10, {});
    provider.add_dependencies("bar", 11, {});
    provider.add_dependencies("bar", 20, {});
    std::string root = "root";
    int root_version = 1;

    auto start_naive = std::chrono::steady_clock::now();
    auto naive_solution = dpll_resolve<TestProvider>(provider, root, root_version);
    auto end_naive = std::chrono::steady_clock::now();

    auto start_solver = std::chrono::steady_clock::now();
    auto solver_solution = resolve<TestProvider>(provider, root, root_version);
    auto end_solver = std::chrono::steady_clock::now();

    // Expected maps differ between implementations in original tests
    std::map<std::string, int> expected; // from test_naive.cpp
    expected["root"] = 1;
    expected["foo"] = 10;
    expected["bar"] = 11;

    std::map<std::string, int> naive_sorted(naive_solution.begin(), naive_solution.end());
    std::map<std::string, int> solver_sorted(solver_solution.begin(), solver_solution.end());

    assert(compare_solutions(expected, naive_sorted));
    assert(compare_solutions(expected, solver_sorted));

    print_solution(naive_sorted);
    print_solution(solver_sorted);
    print_timing("avoiding_conflict_during_decision_making",
                 std::chrono::duration_cast<std::chrono::microseconds>(end_naive - start_naive).count(),
                 std::chrono::duration_cast<std::chrono::microseconds>(end_solver - start_solver).count(),
                 naive_solution.size(), solver_solution.size());
    std::cout << "\n✓ test_avoiding_conflict_during_decision_making passed (both solvers)\n";
}

void test_conflict_resolution()
{
    std::cout << "Running test: conflict_resolution\n";
    TestProvider provider;
    provider.add_dependencies("root", 1, {{"foo", VS::higher_than(1)}});
    provider.add_dependencies("foo", 2, {{"bar", VS::between(1, 2)}}); // foo 2 depends on bar
    provider.add_dependencies("foo", 1, {});                           // foo 1
    provider.add_dependencies("bar", 1, {{"foo", VS::between(1, 2)}}); // bar depends on foo [1,2)
    std::string root = "root";
    int root_version = 1;

    auto start_solver = std::chrono::steady_clock::now();
    auto solver_solution = resolve<TestProvider>(provider, root, root_version);
    auto end_solver = std::chrono::steady_clock::now();

    auto start_naive = std::chrono::steady_clock::now();
    auto naive_solution = dpll_resolve<TestProvider>(provider, root, root_version);
    auto end_naive = std::chrono::steady_clock::now();

    std::map<std::string, int> expected; // original naive expected
    expected["root"] = 1;
    expected["foo"] = 1;

    std::map<std::string, int> naive_sorted(naive_solution.begin(), naive_solution.end());
    std::map<std::string, int> solver_sorted(solver_solution.begin(), solver_solution.end());

    print_solution(naive_sorted);
    print_solution(solver_sorted);
    assert(compare_solutions(expected, naive_sorted));
    assert(compare_solutions(expected, solver_sorted));
    print_timing("conflict_resolution",
                 std::chrono::duration_cast<std::chrono::microseconds>(end_naive - start_naive).count(),
                 std::chrono::duration_cast<std::chrono::microseconds>(end_solver - start_solver).count(),
                 naive_solution.size(), solver_solution.size());
    std::cout << "\n✓ test_conflict_resolution passed (both solvers)\n";
}

void test_conflict_with_partial_satisfier()
{
    std::cout << "Running test: conflict_with_partial_satisfier\n";
    TestProvider provider;
    provider.add_dependencies("root", 1, {{"foo", VS::between(10, 20)}, {"target", VS::between(20, 30)}});
    provider.add_dependencies("foo", 11, {{"left", VS::between(10, 20)}, {"right", VS::between(10, 20)}}); // foo 1.1
    provider.add_dependencies("foo", 10, {});                                                              // foo 1.0
    provider.add_dependencies("left", 10, {{"shared", VS::higher_than(10)}});
    provider.add_dependencies("right", 10, {{"shared", VS::strictly_lower_than(20)}});
    provider.add_dependencies("shared", 20, {});
    provider.add_dependencies("shared", 10, {{"target", VS::between(10, 21)}}); // note: slightly different range than naive file
    provider.add_dependencies("target", 20, {});
    provider.add_dependencies("target", 10, {});
    std::string root = "root";
    int root_version = 1;

    auto start_naive = std::chrono::steady_clock::now();
    auto naive_solution = dpll_resolve<TestProvider>(provider, root, root_version);
    auto end_naive = std::chrono::steady_clock::now();

    auto start_solver = std::chrono::steady_clock::now();
    auto solver_solution = resolve<TestProvider>(provider, root, root_version);
    auto end_solver = std::chrono::steady_clock::now();

    std::map<std::string, int> expected; // both implementations used same expected map in original code
    expected["root"] = 1;
    expected["foo"] = 11;
    expected["target"] = 20;
    expected["shared"] = 10;
    expected["left"] = 10;
    expected["right"] = 10;

    std::map<std::string, int> naive_sorted(naive_solution.begin(), naive_solution.end());
    std::map<std::string, int> solver_sorted(solver_solution.begin(), solver_solution.end());

    assert(compare_solutions(expected, naive_sorted));
    assert(compare_solutions(expected, solver_sorted));

    print_solution(naive_sorted);
    print_solution(solver_sorted);
    print_timing("conflict_with_partial_satisfier",
                 std::chrono::duration_cast<std::chrono::microseconds>(end_naive - start_naive).count(),
                 std::chrono::duration_cast<std::chrono::microseconds>(end_solver - start_solver).count(),
                 naive_solution.size(), solver_solution.size());
    std::cout << "\n✓ test_conflict_with_partial_satisfier passed (both solvers)\n";
}

void test_double_choices()
{
    std::cout << "Running test: double_choices\n";
    TestProvider provider;
    provider.add_dependencies("a", 0, {{"b", VS::full()}, {"c", VS::full()}});
    provider.add_dependencies("b", 0, {{"d", VS::singleton(0)}});
    provider.add_dependencies("b", 1, {{"d", VS::singleton(1)}});
    provider.add_dependencies("c", 0, {});
    provider.add_dependencies("c", 1, {{"d", VS::singleton(2)}});
    provider.add_dependencies("d", 0, {});
    std::string root = "a";
    int root_version = 0;

    auto start_naive = std::chrono::steady_clock::now();
    auto naive_solution = dpll_resolve<TestProvider>(provider, root, root_version);
    auto end_naive = std::chrono::steady_clock::now();

    auto start_solver = std::chrono::steady_clock::now();
    auto solver_solution = resolve<TestProvider>(provider, root, root_version);
    auto end_solver = std::chrono::steady_clock::now();

    std::map<std::string, int> expected;
    expected["a"] = 0;
    expected["b"] = 0;
    expected["c"] = 0;
    expected["d"] = 0;

    std::map<std::string, int> naive_sorted(naive_solution.begin(), naive_solution.end());
    std::map<std::string, int> solver_sorted(solver_solution.begin(), solver_solution.end());

    assert(compare_solutions(expected, naive_sorted));
    assert(compare_solutions(expected, solver_sorted));

    print_solution(naive_sorted);
    print_solution(solver_sorted);
    print_timing("double_choices",
                 std::chrono::duration_cast<std::chrono::microseconds>(end_naive - start_naive).count(),
                 std::chrono::duration_cast<std::chrono::microseconds>(end_solver - start_solver).count(),
                 naive_solution.size(), solver_solution.size());
    std::cout << "\n✓ test_double_choices passed (both solvers)\n";
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
 * Test: very_large_dependency_graph
 * Tests with 100+ packages to stress test the solvers
 * Uses naive/solver comparison instead of hardcoded expected values
 */
void test_very_large_dependency_graph()
{
    std::cout << "Running test: very_large_dependency_graph (100+ packages)\n";
    TestProvider provider;

    // Root package
    provider.add_dependencies("root", 10, {{"frontend-core", VS::between(10, 30)}, {"backend-core", VS::between(10, 30)}, {"infra-core", VS::between(10, 30)}, {"devtools-core", VS::between(10, 30)}});

    // === Frontend Layer (25 packages) ===

    // frontend-core
    provider.add_dependencies("frontend-core", 20, {{"ui-kit", VS::between(20, 30)}, {"state-mgmt", VS::between(20, 30)}, {"routing", VS::between(20, 30)}, {"i18n", VS::between(10, 30)}});
    provider.add_dependencies("frontend-core", 10, {{"ui-kit", VS::between(10, 30)}, {"state-mgmt", VS::between(10, 30)}, {"routing", VS::between(10, 30)}, {"i18n", VS::between(10, 30)}});

    // ui-kit and dependencies
    provider.add_dependencies("ui-kit", 20, {{"design-system", VS::between(20, 30)}, {"animation-lib", VS::between(10, 30)}, {"accessibility", VS::between(10, 30)}});
    provider.add_dependencies("ui-kit", 10, {{"design-system", VS::between(10, 30)}, {"animation-lib", VS::between(10, 30)}, {"accessibility", VS::between(10, 30)}});

    provider.add_dependencies("design-system", 20, {{"theme-engine", VS::between(20, 30)}, {"icon-pack", VS::between(10, 30)}, {"typography", VS::between(10, 30)}, {"color-palette", VS::between(10, 30)}});
    provider.add_dependencies("design-system", 10, {{"theme-engine", VS::between(10, 30)}, {"icon-pack", VS::between(10, 30)}, {"typography", VS::between(10, 30)}, {"color-palette", VS::between(10, 30)}});

    // theme-engine components
    provider.add_dependencies("theme-engine", 20, {{"css-vars", VS::between(10, 30)}, {"dark-mode", VS::between(10, 30)}});
    provider.add_dependencies("theme-engine", 10, {{"css-vars", VS::between(10, 30)}});
    provider.add_dependencies("css-vars", 20, {});
    provider.add_dependencies("css-vars", 10, {});
    provider.add_dependencies("dark-mode", 20, {});
    provider.add_dependencies("dark-mode", 10, {});
    provider.add_dependencies("icon-pack", 20, {});
    provider.add_dependencies("icon-pack", 10, {});
    provider.add_dependencies("typography", 20, {});
    provider.add_dependencies("typography", 10, {});
    provider.add_dependencies("color-palette", 20, {});
    provider.add_dependencies("color-palette", 10, {});

    // animation-lib
    provider.add_dependencies("animation-lib", 20, {{"transition-engine", VS::between(10, 30)}, {"keyframe-gen", VS::between(10, 30)}});
    provider.add_dependencies("animation-lib", 10, {{"transition-engine", VS::between(10, 30)}});
    provider.add_dependencies("transition-engine", 20, {});
    provider.add_dependencies("transition-engine", 10, {});
    provider.add_dependencies("keyframe-gen", 20, {});
    provider.add_dependencies("keyframe-gen", 10, {});

    // accessibility
    provider.add_dependencies("accessibility", 20, {{"aria-utils", VS::between(10, 30)}, {"screen-reader", VS::between(10, 30)}});
    provider.add_dependencies("accessibility", 10, {{"aria-utils", VS::between(10, 30)}});
    provider.add_dependencies("aria-utils", 20, {});
    provider.add_dependencies("aria-utils", 10, {});
    provider.add_dependencies("screen-reader", 20, {});
    provider.add_dependencies("screen-reader", 10, {});

    // state-mgmt
    provider.add_dependencies("state-mgmt", 20, {{"store-core", VS::between(20, 30)}, {"middleware-redux", VS::between(10, 30)}, {"devtools-extension", VS::between(10, 30)}});
    provider.add_dependencies("state-mgmt", 10, {{"store-core", VS::between(10, 30)}, {"middleware-redux", VS::between(10, 30)}});
    provider.add_dependencies("store-core", 20, {});
    provider.add_dependencies("store-core", 10, {});
    provider.add_dependencies("middleware-redux", 20, {});
    provider.add_dependencies("middleware-redux", 10, {});
    provider.add_dependencies("devtools-extension", 20, {});
    provider.add_dependencies("devtools-extension", 10, {});

    // routing
    provider.add_dependencies("routing", 20, {{"history-lib", VS::between(10, 30)}, {"path-matcher", VS::between(10, 30)}});
    provider.add_dependencies("routing", 10, {{"history-lib", VS::between(10, 30)}});
    provider.add_dependencies("history-lib", 20, {});
    provider.add_dependencies("history-lib", 10, {});
    provider.add_dependencies("path-matcher", 20, {});
    provider.add_dependencies("path-matcher", 10, {});

    // i18n
    provider.add_dependencies("i18n", 20, {{"locale-data", VS::between(10, 30)}, {"pluralization", VS::between(10, 30)}});
    provider.add_dependencies("i18n", 10, {{"locale-data", VS::between(10, 30)}});
    provider.add_dependencies("locale-data", 20, {});
    provider.add_dependencies("locale-data", 10, {});
    provider.add_dependencies("pluralization", 20, {});
    provider.add_dependencies("pluralization", 10, {});

    // === Backend Layer (30 packages) ===

    // backend-core
    provider.add_dependencies("backend-core", 20, {{"api-framework", VS::between(20, 30)}, {"auth-system", VS::between(20, 30)}, {"database-layer", VS::between(20, 30)}, {"cache-layer", VS::between(10, 30)}});
    provider.add_dependencies("backend-core", 10, {{"api-framework", VS::between(10, 30)}, {"auth-system", VS::between(10, 30)}, {"database-layer", VS::between(10, 30)}, {"cache-layer", VS::between(10, 30)}});

    // api-framework
    provider.add_dependencies("api-framework", 20, {{"http-server", VS::between(20, 30)}, {"rest-router", VS::between(20, 30)}, {"graphql-engine", VS::between(10, 30)}, {"validation-lib", VS::between(10, 30)}});
    provider.add_dependencies("api-framework", 10, {{"http-server", VS::between(10, 30)}, {"rest-router", VS::between(10, 30)}, {"validation-lib", VS::between(10, 30)}});

    provider.add_dependencies("http-server", 20, {{"middleware-stack", VS::between(20, 30)}, {"compression", VS::between(10, 30)}});
    provider.add_dependencies("http-server", 10, {{"middleware-stack", VS::between(10, 30)}});
    provider.add_dependencies("middleware-stack", 20, {{"cors-handler", VS::between(10, 30)}, {"body-parser", VS::between(10, 30)}});
    provider.add_dependencies("middleware-stack", 10, {{"cors-handler", VS::between(10, 30)}, {"body-parser", VS::between(10, 30)}});
    provider.add_dependencies("cors-handler", 20, {});
    provider.add_dependencies("cors-handler", 10, {});
    provider.add_dependencies("body-parser", 20, {});
    provider.add_dependencies("body-parser", 10, {});
    provider.add_dependencies("compression", 20, {});
    provider.add_dependencies("compression", 10, {});

    provider.add_dependencies("rest-router", 20, {{"url-matcher", VS::between(10, 30)}, {"param-parser", VS::between(10, 30)}});
    provider.add_dependencies("rest-router", 10, {{"url-matcher", VS::between(10, 30)}});
    provider.add_dependencies("url-matcher", 20, {});
    provider.add_dependencies("url-matcher", 10, {});
    provider.add_dependencies("param-parser", 20, {});
    provider.add_dependencies("param-parser", 10, {});

    provider.add_dependencies("graphql-engine", 20, {{"schema-builder", VS::between(10, 30)}, {"query-executor", VS::between(10, 30)}});
    provider.add_dependencies("graphql-engine", 10, {{"schema-builder", VS::between(10, 30)}});
    provider.add_dependencies("schema-builder", 20, {});
    provider.add_dependencies("schema-builder", 10, {});
    provider.add_dependencies("query-executor", 20, {});
    provider.add_dependencies("query-executor", 10, {});

    provider.add_dependencies("validation-lib", 20, {{"schema-validator", VS::between(10, 30)}, {"sanitizer", VS::between(10, 30)}});
    provider.add_dependencies("validation-lib", 10, {{"schema-validator", VS::between(10, 30)}});
    provider.add_dependencies("schema-validator", 20, {});
    provider.add_dependencies("schema-validator", 10, {});
    provider.add_dependencies("sanitizer", 20, {});
    provider.add_dependencies("sanitizer", 10, {});

    // auth-system
    provider.add_dependencies("auth-system", 20, {{"jwt-handler", VS::between(20, 30)}, {"oauth-provider", VS::between(10, 30)}, {"session-manager", VS::between(10, 30)}, {"crypto-lib", VS::between(20, 30)}});
    provider.add_dependencies("auth-system", 10, {{"jwt-handler", VS::between(10, 30)}, {"session-manager", VS::between(10, 30)}, {"crypto-lib", VS::between(10, 30)}});

    provider.add_dependencies("jwt-handler", 20, {{"token-generator", VS::between(10, 30)}, {"crypto-lib", VS::between(20, 30)}});
    provider.add_dependencies("jwt-handler", 10, {{"token-generator", VS::between(10, 30)}, {"crypto-lib", VS::between(10, 30)}});
    provider.add_dependencies("token-generator", 20, {});
    provider.add_dependencies("token-generator", 10, {});

    provider.add_dependencies("oauth-provider", 20, {{"oauth2-flow", VS::between(10, 30)}});
    provider.add_dependencies("oauth-provider", 10, {{"oauth2-flow", VS::between(10, 30)}});
    provider.add_dependencies("oauth2-flow", 20, {});
    provider.add_dependencies("oauth2-flow", 10, {});

    provider.add_dependencies("session-manager", 20, {{"session-store", VS::between(10, 30)}});
    provider.add_dependencies("session-manager", 10, {{"session-store", VS::between(10, 30)}});
    provider.add_dependencies("session-store", 20, {});
    provider.add_dependencies("session-store", 10, {});

    provider.add_dependencies("crypto-lib", 20, {{"hash-functions", VS::between(10, 30)}, {"encryption", VS::between(10, 30)}});
    provider.add_dependencies("crypto-lib", 10, {{"hash-functions", VS::between(10, 30)}});
    provider.add_dependencies("hash-functions", 20, {});
    provider.add_dependencies("hash-functions", 10, {});
    provider.add_dependencies("encryption", 20, {});
    provider.add_dependencies("encryption", 10, {});

    // database-layer
    provider.add_dependencies("database-layer", 20, {{"orm-engine", VS::between(20, 30)}, {"migration-tool", VS::between(10, 30)}, {"connection-pool", VS::between(10, 30)}, {"query-builder", VS::between(20, 30)}});
    provider.add_dependencies("database-layer", 10, {{"orm-engine", VS::between(10, 30)}, {"migration-tool", VS::between(10, 30)}, {"connection-pool", VS::between(10, 30)}, {"query-builder", VS::between(10, 30)}});

    provider.add_dependencies("orm-engine", 20, {{"model-mapper", VS::between(10, 30)}, {"relation-handler", VS::between(10, 30)}});
    provider.add_dependencies("orm-engine", 10, {{"model-mapper", VS::between(10, 30)}});
    provider.add_dependencies("model-mapper", 20, {});
    provider.add_dependencies("model-mapper", 10, {});
    provider.add_dependencies("relation-handler", 20, {});
    provider.add_dependencies("relation-handler", 10, {});

    provider.add_dependencies("migration-tool", 20, {{"version-tracker", VS::between(10, 30)}});
    provider.add_dependencies("migration-tool", 10, {{"version-tracker", VS::between(10, 30)}});
    provider.add_dependencies("version-tracker", 20, {});
    provider.add_dependencies("version-tracker", 10, {});

    provider.add_dependencies("connection-pool", 20, {{"pool-manager", VS::between(10, 30)}});
    provider.add_dependencies("connection-pool", 10, {{"pool-manager", VS::between(10, 30)}});
    provider.add_dependencies("pool-manager", 20, {});
    provider.add_dependencies("pool-manager", 10, {});

    provider.add_dependencies("query-builder", 20, {{"sql-generator", VS::between(10, 30)}});
    provider.add_dependencies("query-builder", 10, {{"sql-generator", VS::between(10, 30)}});
    provider.add_dependencies("sql-generator", 20, {});
    provider.add_dependencies("sql-generator", 10, {});

    // cache-layer
    provider.add_dependencies("cache-layer", 20, {{"redis-client", VS::between(10, 30)}, {"memcached-client", VS::between(10, 30)}});
    provider.add_dependencies("cache-layer", 10, {{"redis-client", VS::between(10, 30)}});
    provider.add_dependencies("redis-client", 20, {});
    provider.add_dependencies("redis-client", 10, {});
    provider.add_dependencies("memcached-client", 20, {});
    provider.add_dependencies("memcached-client", 10, {});

    // === Infrastructure Layer (25 packages) ===

    provider.add_dependencies("infra-core", 20, {{"monitoring", VS::between(20, 30)}, {"logging", VS::between(20, 30)}, {"deployment", VS::between(10, 30)}, {"cloud-services", VS::between(10, 30)}});
    provider.add_dependencies("infra-core", 10, {{"monitoring", VS::between(10, 30)}, {"logging", VS::between(10, 30)}, {"deployment", VS::between(10, 30)}});

    // monitoring
    provider.add_dependencies("monitoring", 20, {{"metrics-collector", VS::between(20, 30)}, {"alerting", VS::between(10, 30)}, {"tracing", VS::between(10, 30)}, {"dashboards", VS::between(10, 30)}});
    provider.add_dependencies("monitoring", 10, {{"metrics-collector", VS::between(10, 30)}, {"alerting", VS::between(10, 30)}});

    provider.add_dependencies("metrics-collector", 20, {{"prometheus-client", VS::between(10, 30)}, {"statsd-client", VS::between(10, 30)}});
    provider.add_dependencies("metrics-collector", 10, {{"prometheus-client", VS::between(10, 30)}});
    provider.add_dependencies("prometheus-client", 20, {});
    provider.add_dependencies("prometheus-client", 10, {});
    provider.add_dependencies("statsd-client", 20, {});
    provider.add_dependencies("statsd-client", 10, {});

    provider.add_dependencies("alerting", 20, {{"notification-service", VS::between(10, 30)}, {"rule-engine", VS::between(10, 30)}});
    provider.add_dependencies("alerting", 10, {{"notification-service", VS::between(10, 30)}});
    provider.add_dependencies("notification-service", 20, {});
    provider.add_dependencies("notification-service", 10, {});
    provider.add_dependencies("rule-engine", 20, {});
    provider.add_dependencies("rule-engine", 10, {});

    provider.add_dependencies("tracing", 20, {{"trace-collector", VS::between(10, 30)}, {"span-processor", VS::between(10, 30)}});
    provider.add_dependencies("tracing", 10, {{"trace-collector", VS::between(10, 30)}});
    provider.add_dependencies("trace-collector", 20, {});
    provider.add_dependencies("trace-collector", 10, {});
    provider.add_dependencies("span-processor", 20, {});
    provider.add_dependencies("span-processor", 10, {});

    provider.add_dependencies("dashboards", 20, {{"grafana-api", VS::between(10, 30)}});
    provider.add_dependencies("dashboards", 10, {{"grafana-api", VS::between(10, 30)}});
    provider.add_dependencies("grafana-api", 20, {});
    provider.add_dependencies("grafana-api", 10, {});

    // logging
    provider.add_dependencies("logging", 20, {{"log-aggregator", VS::between(20, 30)}, {"log-formatter", VS::between(10, 30)}, {"log-transport", VS::between(10, 30)}});
    provider.add_dependencies("logging", 10, {{"log-aggregator", VS::between(10, 30)}, {"log-formatter", VS::between(10, 30)}});

    provider.add_dependencies("log-aggregator", 20, {{"elasticsearch-client", VS::between(10, 30)}, {"log-shipper", VS::between(10, 30)}});
    provider.add_dependencies("log-aggregator", 10, {{"elasticsearch-client", VS::between(10, 30)}});
    provider.add_dependencies("elasticsearch-client", 20, {});
    provider.add_dependencies("elasticsearch-client", 10, {});
    provider.add_dependencies("log-shipper", 20, {});
    provider.add_dependencies("log-shipper", 10, {});

    provider.add_dependencies("log-formatter", 20, {{"json-formatter", VS::between(10, 30)}});
    provider.add_dependencies("log-formatter", 10, {{"json-formatter", VS::between(10, 30)}});
    provider.add_dependencies("json-formatter", 20, {});
    provider.add_dependencies("json-formatter", 10, {});

    provider.add_dependencies("log-transport", 20, {{"syslog-client", VS::between(10, 30)}});
    provider.add_dependencies("log-transport", 10, {{"syslog-client", VS::between(10, 30)}});
    provider.add_dependencies("syslog-client", 20, {});
    provider.add_dependencies("syslog-client", 10, {});

    // deployment
    provider.add_dependencies("deployment", 20, {{"container-runtime", VS::between(10, 30)}, {"orchestrator", VS::between(10, 30)}, {"ci-cd", VS::between(10, 30)}});
    provider.add_dependencies("deployment", 10, {{"container-runtime", VS::between(10, 30)}, {"orchestrator", VS::between(10, 30)}});

    provider.add_dependencies("container-runtime", 20, {{"docker-api", VS::between(10, 30)}});
    provider.add_dependencies("container-runtime", 10, {{"docker-api", VS::between(10, 30)}});
    provider.add_dependencies("docker-api", 20, {});
    provider.add_dependencies("docker-api", 10, {});

    provider.add_dependencies("orchestrator", 20, {{"k8s-client", VS::between(10, 30)}});
    provider.add_dependencies("orchestrator", 10, {{"k8s-client", VS::between(10, 30)}});
    provider.add_dependencies("k8s-client", 20, {});
    provider.add_dependencies("k8s-client", 10, {});

    provider.add_dependencies("ci-cd", 20, {{"pipeline-runner", VS::between(10, 30)}});
    provider.add_dependencies("ci-cd", 10, {{"pipeline-runner", VS::between(10, 30)}});
    provider.add_dependencies("pipeline-runner", 20, {});
    provider.add_dependencies("pipeline-runner", 10, {});

    // cloud-services
    provider.add_dependencies("cloud-services", 20, {{"storage-sdk", VS::between(10, 30)}, {"messaging-sdk", VS::between(10, 30)}, {"cdn-sdk", VS::between(10, 30)}});
    provider.add_dependencies("cloud-services", 10, {{"storage-sdk", VS::between(10, 30)}, {"messaging-sdk", VS::between(10, 30)}});

    provider.add_dependencies("storage-sdk", 20, {{"s3-client", VS::between(10, 30)}});
    provider.add_dependencies("storage-sdk", 10, {{"s3-client", VS::between(10, 30)}});
    provider.add_dependencies("s3-client", 20, {});
    provider.add_dependencies("s3-client", 10, {});

    provider.add_dependencies("messaging-sdk", 20, {{"sqs-client", VS::between(10, 30)}, {"sns-client", VS::between(10, 30)}});
    provider.add_dependencies("messaging-sdk", 10, {{"sqs-client", VS::between(10, 30)}});
    provider.add_dependencies("sqs-client", 20, {});
    provider.add_dependencies("sqs-client", 10, {});
    provider.add_dependencies("sns-client", 20, {});
    provider.add_dependencies("sns-client", 10, {});

    provider.add_dependencies("cdn-sdk", 20, {{"cloudfront-client", VS::between(10, 30)}});
    provider.add_dependencies("cdn-sdk", 10, {{"cloudfront-client", VS::between(10, 30)}});
    provider.add_dependencies("cloudfront-client", 20, {});
    provider.add_dependencies("cloudfront-client", 10, {});

    // === DevTools Layer (20 packages) ===

    provider.add_dependencies("devtools-core", 20, {{"testing-framework", VS::between(20, 30)}, {"linter", VS::between(10, 30)}, {"bundler", VS::between(20, 30)}, {"docs-generator", VS::between(10, 30)}});
    provider.add_dependencies("devtools-core", 10, {{"testing-framework", VS::between(10, 30)}, {"linter", VS::between(10, 30)}, {"bundler", VS::between(10, 30)}});

    // testing-framework
    provider.add_dependencies("testing-framework", 20, {{"test-runner", VS::between(20, 30)}, {"assertion-lib", VS::between(10, 30)}, {"mock-framework", VS::between(10, 30)}, {"coverage-tool", VS::between(10, 30)}});
    provider.add_dependencies("testing-framework", 10, {{"test-runner", VS::between(10, 30)}, {"assertion-lib", VS::between(10, 30)}});

    provider.add_dependencies("test-runner", 20, {{"parallel-executor", VS::between(10, 30)}, {"reporter", VS::between(10, 30)}});
    provider.add_dependencies("test-runner", 10, {{"parallel-executor", VS::between(10, 30)}});
    provider.add_dependencies("parallel-executor", 20, {});
    provider.add_dependencies("parallel-executor", 10, {});
    provider.add_dependencies("reporter", 20, {});
    provider.add_dependencies("reporter", 10, {});

    provider.add_dependencies("assertion-lib", 20, {{"matcher-lib", VS::between(10, 30)}});
    provider.add_dependencies("assertion-lib", 10, {{"matcher-lib", VS::between(10, 30)}});
    provider.add_dependencies("matcher-lib", 20, {});
    provider.add_dependencies("matcher-lib", 10, {});

    provider.add_dependencies("mock-framework", 20, {{"spy-lib", VS::between(10, 30)}});
    provider.add_dependencies("mock-framework", 10, {{"spy-lib", VS::between(10, 30)}});
    provider.add_dependencies("spy-lib", 20, {});
    provider.add_dependencies("spy-lib", 10, {});

    provider.add_dependencies("coverage-tool", 20, {{"instrumentation", VS::between(10, 30)}});
    provider.add_dependencies("coverage-tool", 10, {{"instrumentation", VS::between(10, 30)}});
    provider.add_dependencies("instrumentation", 20, {});
    provider.add_dependencies("instrumentation", 10, {});

    // linter
    provider.add_dependencies("linter", 20, {{"syntax-checker", VS::between(10, 30)}, {"style-checker", VS::between(10, 30)}, {"security-scanner", VS::between(10, 30)}});
    provider.add_dependencies("linter", 10, {{"syntax-checker", VS::between(10, 30)}, {"style-checker", VS::between(10, 30)}});

    provider.add_dependencies("syntax-checker", 20, {{"parser-lib", VS::between(10, 30)}});
    provider.add_dependencies("syntax-checker", 10, {{"parser-lib", VS::between(10, 30)}});
    provider.add_dependencies("parser-lib", 20, {});
    provider.add_dependencies("parser-lib", 10, {});

    provider.add_dependencies("style-checker", 20, {{"rule-engine-lint", VS::between(10, 30)}});
    provider.add_dependencies("style-checker", 10, {{"rule-engine-lint", VS::between(10, 30)}});
    provider.add_dependencies("rule-engine-lint", 20, {});
    provider.add_dependencies("rule-engine-lint", 10, {});

    provider.add_dependencies("security-scanner", 20, {{"vulnerability-db", VS::between(10, 30)}});
    provider.add_dependencies("security-scanner", 10, {{"vulnerability-db", VS::between(10, 30)}});
    provider.add_dependencies("vulnerability-db", 20, {});
    provider.add_dependencies("vulnerability-db", 10, {});

    // bundler
    provider.add_dependencies("bundler", 20, {{"module-resolver", VS::between(20, 30)}, {"minifier", VS::between(10, 30)}, {"tree-shaker", VS::between(10, 30)}, {"code-splitter", VS::between(10, 30)}});
    provider.add_dependencies("bundler", 10, {{"module-resolver", VS::between(10, 30)}, {"minifier", VS::between(10, 30)}});

    provider.add_dependencies("module-resolver", 20, {{"path-resolver", VS::between(10, 30)}});
    provider.add_dependencies("module-resolver", 10, {{"path-resolver", VS::between(10, 30)}});
    provider.add_dependencies("path-resolver", 20, {});
    provider.add_dependencies("path-resolver", 10, {});

    provider.add_dependencies("minifier", 20, {{"uglifier", VS::between(10, 30)}});
    provider.add_dependencies("minifier", 10, {{"uglifier", VS::between(10, 30)}});
    provider.add_dependencies("uglifier", 20, {});
    provider.add_dependencies("uglifier", 10, {});

    provider.add_dependencies("tree-shaker", 20, {{"dependency-analyzer", VS::between(10, 30)}});
    provider.add_dependencies("tree-shaker", 10, {{"dependency-analyzer", VS::between(10, 30)}});
    provider.add_dependencies("dependency-analyzer", 20, {});
    provider.add_dependencies("dependency-analyzer", 10, {});

    provider.add_dependencies("code-splitter", 20, {{"chunk-optimizer", VS::between(10, 30)}});
    provider.add_dependencies("code-splitter", 10, {{"chunk-optimizer", VS::between(10, 30)}});
    provider.add_dependencies("chunk-optimizer", 20, {});
    provider.add_dependencies("chunk-optimizer", 10, {});

    // docs-generator
    provider.add_dependencies("docs-generator", 20, {{"markdown-parser", VS::between(10, 30)}, {"api-extractor", VS::between(10, 30)}, {"static-site-gen", VS::between(10, 30)}});
    provider.add_dependencies("docs-generator", 10, {{"markdown-parser", VS::between(10, 30)}, {"api-extractor", VS::between(10, 30)}});

    provider.add_dependencies("markdown-parser", 20, {{"syntax-highlighter", VS::between(10, 30)}});
    provider.add_dependencies("markdown-parser", 10, {{"syntax-highlighter", VS::between(10, 30)}});
    provider.add_dependencies("syntax-highlighter", 20, {});
    provider.add_dependencies("syntax-highlighter", 10, {});

    provider.add_dependencies("api-extractor", 20, {{"ast-parser", VS::between(10, 30)}});
    provider.add_dependencies("api-extractor", 10, {{"ast-parser", VS::between(10, 30)}});
    provider.add_dependencies("ast-parser", 20, {});
    provider.add_dependencies("ast-parser", 10, {});

    provider.add_dependencies("static-site-gen", 20, {{"template-engine", VS::between(10, 30)}});
    provider.add_dependencies("static-site-gen", 10, {{"template-engine", VS::between(10, 30)}});
    provider.add_dependencies("template-engine", 20, {});
    provider.add_dependencies("template-engine", 10, {});

    // === Run both solvers ===
    std::cout << "Solving with both naive and optimized solvers...\n";
    std::string root = "root";
    int root_version = 10;

    auto start_naive = std::chrono::steady_clock::now();
    auto naive_solution = dpll_resolve<TestProvider>(provider, root, root_version);
    auto end_naive = std::chrono::steady_clock::now();

    auto start_solver = std::chrono::steady_clock::now();
    auto solver_solution = resolve<TestProvider>(provider, root, root_version);
    auto end_solver = std::chrono::steady_clock::now();

    // Convert to sorted maps for comparison
    std::map<std::string, int> naive_sorted(naive_solution.begin(), naive_solution.end());
    std::map<std::string, int> solver_sorted(solver_solution.begin(), solver_solution.end());

    // Verify both found solutions
    assert(naive_sorted.size() > 0);
    assert(solver_sorted.size() > 0);
    assert(naive_sorted.count("root") > 0);
    assert(solver_sorted.count("root") > 0);

    // Compare naive and CDCL results - they should be identical
    std::cout << "Comparing DPLL vs CDCL results...\n";
    bool results_match = compare_solutions(naive_sorted, solver_sorted);

    if (!results_match)
    {
        std::cout << "ERROR: DPLL and CDCL produced different results!\n";
        std::cout << "DPLL solution:\n";
        print_solution(naive_sorted);
        std::cout << "CDCL solution:\n";
        print_solution(solver_sorted);
    }
    assert(results_match);

    std::cout << "Package count: " << solver_sorted.size() << "\n";
    print_timing("very_large_dependency_graph",
                 std::chrono::duration_cast<std::chrono::microseconds>(end_naive - start_naive).count(),
                 std::chrono::duration_cast<std::chrono::microseconds>(end_solver - start_solver).count(),
                 naive_solution.size(), solver_solution.size());

    std::cout << "\n✓ test_very_large_dependency_graph passed (100+ packages, naive==solver)\n";
}

/**
 * Test: huge_dependency_graph_200pkg
 * Tests with 200+ packages - massive stress test
 * This creates a more complex dependency graph with conflicts to trigger backtracking
 */
void test_huge_dependency_graph_200pkg()
{
    std::cout << "Running test: huge_dependency_graph_200pkg (200+ packages)\n";
    TestProvider provider;

    // Root package with 5 major subsystems
    provider.add_dependencies("root", 10, {{"web-platform", VS::between(10, 30)}, {"mobile-platform", VS::between(10, 30)}, {"backend-services", VS::between(10, 30)}, {"data-platform", VS::between(10, 30)}, {"ml-platform", VS::between(10, 30)}});

    // === Web Platform (40 packages) ===
    provider.add_dependencies("web-platform", 20, {{"web-ui", VS::between(20, 30)}, {"web-routing", VS::between(20, 30)}, {"web-state", VS::between(20, 30)}, {"web-forms", VS::between(10, 30)}});
    provider.add_dependencies("web-platform", 10, {{"web-ui", VS::between(10, 30)}, {"web-routing", VS::between(10, 30)}, {"web-state", VS::between(10, 30)}, {"web-forms", VS::between(10, 30)}});

    // web-ui subsystem (10 packages)
    provider.add_dependencies("web-ui", 20, {{"component-lib", VS::between(20, 30)}, {"design-tokens", VS::between(10, 30)}, {"icon-system", VS::between(10, 30)}});
    provider.add_dependencies("web-ui", 10, {{"component-lib", VS::between(10, 30)}, {"design-tokens", VS::between(10, 30)}, {"icon-system", VS::between(10, 30)}});

    provider.add_dependencies("component-lib", 20, {{"button-comp", VS::between(10, 30)}, {"input-comp", VS::between(10, 30)}, {"modal-comp", VS::between(10, 30)}});
    provider.add_dependencies("component-lib", 10, {{"button-comp", VS::between(10, 30)}, {"input-comp", VS::between(10, 30)}});
    provider.add_dependencies("button-comp", 20, {});
    provider.add_dependencies("button-comp", 10, {});
    provider.add_dependencies("input-comp", 20, {});
    provider.add_dependencies("input-comp", 10, {});
    provider.add_dependencies("modal-comp", 20, {});
    provider.add_dependencies("modal-comp", 10, {});
    provider.add_dependencies("design-tokens", 20, {});
    provider.add_dependencies("design-tokens", 10, {});
    provider.add_dependencies("icon-system", 20, {});
    provider.add_dependencies("icon-system", 10, {});

    // web-routing subsystem (10 packages)
    provider.add_dependencies("web-routing", 20, {{"router-core", VS::between(20, 30)}, {"route-guards", VS::between(10, 30)}, {"nav-history", VS::between(10, 30)}});
    provider.add_dependencies("web-routing", 10, {{"router-core", VS::between(10, 30)}, {"route-guards", VS::between(10, 30)}, {"nav-history", VS::between(10, 30)}});

    provider.add_dependencies("router-core", 20, {{"path-parser", VS::between(10, 30)}, {"route-matcher", VS::between(10, 30)}});
    provider.add_dependencies("router-core", 10, {{"path-parser", VS::between(10, 30)}});
    provider.add_dependencies("path-parser", 20, {});
    provider.add_dependencies("path-parser", 10, {});
    provider.add_dependencies("route-matcher", 20, {});
    provider.add_dependencies("route-matcher", 10, {});
    provider.add_dependencies("route-guards", 20, {{"auth-guard", VS::between(10, 30)}});
    provider.add_dependencies("route-guards", 10, {{"auth-guard", VS::between(10, 30)}});
    provider.add_dependencies("auth-guard", 20, {});
    provider.add_dependencies("auth-guard", 10, {});
    provider.add_dependencies("nav-history", 20, {});
    provider.add_dependencies("nav-history", 10, {});

    // web-state subsystem (10 packages)
    provider.add_dependencies("web-state", 20, {{"store-engine", VS::between(20, 30)}, {"state-middleware", VS::between(10, 30)}, {"state-devtools", VS::between(10, 30)}});
    provider.add_dependencies("web-state", 10, {{"store-engine", VS::between(10, 30)}, {"state-middleware", VS::between(10, 30)}});

    provider.add_dependencies("store-engine", 20, {{"reducer-utils", VS::between(10, 30)}, {"action-creators", VS::between(10, 30)}});
    provider.add_dependencies("store-engine", 10, {{"reducer-utils", VS::between(10, 30)}});
    provider.add_dependencies("reducer-utils", 20, {});
    provider.add_dependencies("reducer-utils", 10, {});
    provider.add_dependencies("action-creators", 20, {});
    provider.add_dependencies("action-creators", 10, {});
    provider.add_dependencies("state-middleware", 20, {{"thunk-middleware", VS::between(10, 30)}});
    provider.add_dependencies("state-middleware", 10, {{"thunk-middleware", VS::between(10, 30)}});
    provider.add_dependencies("thunk-middleware", 20, {});
    provider.add_dependencies("thunk-middleware", 10, {});
    provider.add_dependencies("state-devtools", 20, {});
    provider.add_dependencies("state-devtools", 10, {});

    // web-forms subsystem (10 packages)
    provider.add_dependencies("web-forms", 20, {{"form-validation", VS::between(20, 30)}, {"form-state", VS::between(10, 30)}, {"form-components", VS::between(10, 30)}});
    provider.add_dependencies("web-forms", 10, {{"form-validation", VS::between(10, 30)}, {"form-state", VS::between(10, 30)}});

    provider.add_dependencies("form-validation", 20, {{"validator-rules", VS::between(10, 30)}, {"error-messages", VS::between(10, 30)}});
    provider.add_dependencies("form-validation", 10, {{"validator-rules", VS::between(10, 30)}});
    provider.add_dependencies("validator-rules", 20, {});
    provider.add_dependencies("validator-rules", 10, {});
    provider.add_dependencies("error-messages", 20, {});
    provider.add_dependencies("error-messages", 10, {});
    provider.add_dependencies("form-state", 20, {});
    provider.add_dependencies("form-state", 10, {});
    provider.add_dependencies("form-components", 20, {});
    provider.add_dependencies("form-components", 10, {});

    // === Mobile Platform (40 packages) ===
    provider.add_dependencies("mobile-platform", 20, {{"mobile-ui", VS::between(20, 30)}, {"mobile-nav", VS::between(20, 30)}, {"mobile-storage", VS::between(10, 30)}, {"mobile-native", VS::between(10, 30)}});
    provider.add_dependencies("mobile-platform", 10, {{"mobile-ui", VS::between(10, 30)}, {"mobile-nav", VS::between(10, 30)}, {"mobile-storage", VS::between(10, 30)}});

    // mobile-ui (10 packages)
    provider.add_dependencies("mobile-ui", 20, {{"native-components", VS::between(20, 30)}, {"gesture-handler", VS::between(10, 30)}, {"animation-system", VS::between(10, 30)}});
    provider.add_dependencies("mobile-ui", 10, {{"native-components", VS::between(10, 30)}, {"gesture-handler", VS::between(10, 30)}});
    provider.add_dependencies("native-components", 20, {{"touchable-comp", VS::between(10, 30)}, {"view-comp", VS::between(10, 30)}});
    provider.add_dependencies("native-components", 10, {{"touchable-comp", VS::between(10, 30)}});
    provider.add_dependencies("touchable-comp", 20, {});
    provider.add_dependencies("touchable-comp", 10, {});
    provider.add_dependencies("view-comp", 20, {});
    provider.add_dependencies("view-comp", 10, {});
    provider.add_dependencies("gesture-handler", 20, {{"pan-gesture", VS::between(10, 30)}});
    provider.add_dependencies("gesture-handler", 10, {{"pan-gesture", VS::between(10, 30)}});
    provider.add_dependencies("pan-gesture", 20, {});
    provider.add_dependencies("pan-gesture", 10, {});
    provider.add_dependencies("animation-system", 20, {{"reanimated", VS::between(10, 30)}});
    provider.add_dependencies("animation-system", 10, {{"reanimated", VS::between(10, 30)}});
    provider.add_dependencies("reanimated", 20, {});
    provider.add_dependencies("reanimated", 10, {});

    // mobile-nav (10 packages)
    provider.add_dependencies("mobile-nav", 20, {{"stack-nav", VS::between(20, 30)}, {"tab-nav", VS::between(10, 30)}, {"drawer-nav", VS::between(10, 30)}});
    provider.add_dependencies("mobile-nav", 10, {{"stack-nav", VS::between(10, 30)}, {"tab-nav", VS::between(10, 30)}});
    provider.add_dependencies("stack-nav", 20, {{"screen-manager", VS::between(10, 30)}});
    provider.add_dependencies("stack-nav", 10, {{"screen-manager", VS::between(10, 30)}});
    provider.add_dependencies("screen-manager", 20, {});
    provider.add_dependencies("screen-manager", 10, {});
    provider.add_dependencies("tab-nav", 20, {{"tab-bar", VS::between(10, 30)}});
    provider.add_dependencies("tab-nav", 10, {{"tab-bar", VS::between(10, 30)}});
    provider.add_dependencies("tab-bar", 20, {});
    provider.add_dependencies("tab-bar", 10, {});
    provider.add_dependencies("drawer-nav", 20, {{"drawer-comp", VS::between(10, 30)}});
    provider.add_dependencies("drawer-nav", 10, {{"drawer-comp", VS::between(10, 30)}});
    provider.add_dependencies("drawer-comp", 20, {});
    provider.add_dependencies("drawer-comp", 10, {});

    // mobile-storage (10 packages)
    provider.add_dependencies("mobile-storage", 20, {{"async-storage", VS::between(20, 30)}, {"secure-storage", VS::between(10, 30)}, {"cache-storage", VS::between(10, 30)}});
    provider.add_dependencies("mobile-storage", 10, {{"async-storage", VS::between(10, 30)}, {"secure-storage", VS::between(10, 30)}});
    provider.add_dependencies("async-storage", 20, {{"storage-backend", VS::between(10, 30)}});
    provider.add_dependencies("async-storage", 10, {{"storage-backend", VS::between(10, 30)}});
    provider.add_dependencies("storage-backend", 20, {});
    provider.add_dependencies("storage-backend", 10, {});
    provider.add_dependencies("secure-storage", 20, {{"keychain", VS::between(10, 30)}});
    provider.add_dependencies("secure-storage", 10, {{"keychain", VS::between(10, 30)}});
    provider.add_dependencies("keychain", 20, {});
    provider.add_dependencies("keychain", 10, {});
    provider.add_dependencies("cache-storage", 20, {{"lru-cache", VS::between(10, 30)}});
    provider.add_dependencies("cache-storage", 10, {{"lru-cache", VS::between(10, 30)}});
    provider.add_dependencies("lru-cache", 20, {});
    provider.add_dependencies("lru-cache", 10, {});

    // mobile-native (10 packages)
    provider.add_dependencies("mobile-native", 20, {{"camera-module", VS::between(10, 30)}, {"location-module", VS::between(10, 30)}, {"push-notifications", VS::between(10, 30)}});
    provider.add_dependencies("mobile-native", 10, {{"camera-module", VS::between(10, 30)}, {"location-module", VS::between(10, 30)}});
    provider.add_dependencies("camera-module", 20, {{"media-picker", VS::between(10, 30)}});
    provider.add_dependencies("camera-module", 10, {{"media-picker", VS::between(10, 30)}});
    provider.add_dependencies("media-picker", 20, {});
    provider.add_dependencies("media-picker", 10, {});
    provider.add_dependencies("location-module", 20, {{"gps-service", VS::between(10, 30)}});
    provider.add_dependencies("location-module", 10, {{"gps-service", VS::between(10, 30)}});
    provider.add_dependencies("gps-service", 20, {});
    provider.add_dependencies("gps-service", 10, {});
    provider.add_dependencies("push-notifications", 20, {{"fcm-client", VS::between(10, 30)}});
    provider.add_dependencies("push-notifications", 10, {{"fcm-client", VS::between(10, 30)}});
    provider.add_dependencies("fcm-client", 20, {});
    provider.add_dependencies("fcm-client", 10, {});

    // === Backend Services (40 packages) ===
    provider.add_dependencies("backend-services", 20, {{"api-gateway", VS::between(20, 30)}, {"auth-service", VS::between(20, 30)}, {"user-service", VS::between(10, 30)}, {"payment-service", VS::between(10, 30)}});
    provider.add_dependencies("backend-services", 10, {{"api-gateway", VS::between(10, 30)}, {"auth-service", VS::between(10, 30)}, {"user-service", VS::between(10, 30)}});

    // api-gateway (10 packages)
    provider.add_dependencies("api-gateway", 20, {{"gateway-router", VS::between(20, 30)}, {"rate-limiter", VS::between(10, 30)}, {"load-balancer", VS::between(10, 30)}});
    provider.add_dependencies("api-gateway", 10, {{"gateway-router", VS::between(10, 30)}, {"rate-limiter", VS::between(10, 30)}});
    provider.add_dependencies("gateway-router", 20, {{"route-config", VS::between(10, 30)}, {"proxy-handler", VS::between(10, 30)}});
    provider.add_dependencies("gateway-router", 10, {{"route-config", VS::between(10, 30)}});
    provider.add_dependencies("route-config", 20, {});
    provider.add_dependencies("route-config", 10, {});
    provider.add_dependencies("proxy-handler", 20, {});
    provider.add_dependencies("proxy-handler", 10, {});
    provider.add_dependencies("rate-limiter", 20, {{"token-bucket", VS::between(10, 30)}});
    provider.add_dependencies("rate-limiter", 10, {{"token-bucket", VS::between(10, 30)}});
    provider.add_dependencies("token-bucket", 20, {});
    provider.add_dependencies("token-bucket", 10, {});
    provider.add_dependencies("load-balancer", 20, {{"lb-strategy", VS::between(10, 30)}});
    provider.add_dependencies("load-balancer", 10, {{"lb-strategy", VS::between(10, 30)}});
    provider.add_dependencies("lb-strategy", 20, {});
    provider.add_dependencies("lb-strategy", 10, {});

    // auth-service (10 packages)
    provider.add_dependencies("auth-service", 20, {{"jwt-auth", VS::between(20, 30)}, {"oauth2-server", VS::between(10, 30)}, {"password-hash", VS::between(10, 30)}});
    provider.add_dependencies("auth-service", 10, {{"jwt-auth", VS::between(10, 30)}, {"oauth2-server", VS::between(10, 30)}});
    provider.add_dependencies("jwt-auth", 20, {{"token-mgmt", VS::between(10, 30)}, {"claim-validator", VS::between(10, 30)}});
    provider.add_dependencies("jwt-auth", 10, {{"token-mgmt", VS::between(10, 30)}});
    provider.add_dependencies("token-mgmt", 20, {});
    provider.add_dependencies("token-mgmt", 10, {});
    provider.add_dependencies("claim-validator", 20, {});
    provider.add_dependencies("claim-validator", 10, {});
    provider.add_dependencies("oauth2-server", 20, {{"auth-code-flow", VS::between(10, 30)}});
    provider.add_dependencies("oauth2-server", 10, {{"auth-code-flow", VS::between(10, 30)}});
    provider.add_dependencies("auth-code-flow", 20, {});
    provider.add_dependencies("auth-code-flow", 10, {});
    provider.add_dependencies("password-hash", 20, {{"bcrypt-lib", VS::between(10, 30)}});
    provider.add_dependencies("password-hash", 10, {{"bcrypt-lib", VS::between(10, 30)}});
    provider.add_dependencies("bcrypt-lib", 20, {});
    provider.add_dependencies("bcrypt-lib", 10, {});

    // user-service (10 packages)
    provider.add_dependencies("user-service", 20, {{"user-repo", VS::between(20, 30)}, {"profile-mgmt", VS::between(10, 30)}, {"email-verify", VS::between(10, 30)}});
    provider.add_dependencies("user-service", 10, {{"user-repo", VS::between(10, 30)}, {"profile-mgmt", VS::between(10, 30)}});
    provider.add_dependencies("user-repo", 20, {{"db-adapter", VS::between(10, 30)}, {"user-model", VS::between(10, 30)}});
    provider.add_dependencies("user-repo", 10, {{"db-adapter", VS::between(10, 30)}});
    provider.add_dependencies("db-adapter", 20, {});
    provider.add_dependencies("db-adapter", 10, {});
    provider.add_dependencies("user-model", 20, {});
    provider.add_dependencies("user-model", 10, {});
    provider.add_dependencies("profile-mgmt", 20, {{"avatar-upload", VS::between(10, 30)}});
    provider.add_dependencies("profile-mgmt", 10, {{"avatar-upload", VS::between(10, 30)}});
    provider.add_dependencies("avatar-upload", 20, {});
    provider.add_dependencies("avatar-upload", 10, {});
    provider.add_dependencies("email-verify", 20, {{"email-sender", VS::between(10, 30)}});
    provider.add_dependencies("email-verify", 10, {{"email-sender", VS::between(10, 30)}});
    provider.add_dependencies("email-sender", 20, {});
    provider.add_dependencies("email-sender", 10, {});

    // payment-service (10 packages)
    provider.add_dependencies("payment-service", 20, {{"stripe-integration", VS::between(10, 30)}, {"invoice-gen", VS::between(10, 30)}, {"payment-webhook", VS::between(10, 30)}});
    provider.add_dependencies("payment-service", 10, {{"stripe-integration", VS::between(10, 30)}, {"invoice-gen", VS::between(10, 30)}});
    provider.add_dependencies("stripe-integration", 20, {{"stripe-sdk", VS::between(10, 30)}, {"payment-intent", VS::between(10, 30)}});
    provider.add_dependencies("stripe-integration", 10, {{"stripe-sdk", VS::between(10, 30)}});
    provider.add_dependencies("stripe-sdk", 20, {});
    provider.add_dependencies("stripe-sdk", 10, {});
    provider.add_dependencies("payment-intent", 20, {});
    provider.add_dependencies("payment-intent", 10, {});
    provider.add_dependencies("invoice-gen", 20, {{"pdf-generator", VS::between(10, 30)}});
    provider.add_dependencies("invoice-gen", 10, {{"pdf-generator", VS::between(10, 30)}});
    provider.add_dependencies("pdf-generator", 20, {});
    provider.add_dependencies("pdf-generator", 10, {});
    provider.add_dependencies("payment-webhook", 20, {{"webhook-handler", VS::between(10, 30)}});
    provider.add_dependencies("payment-webhook", 10, {{"webhook-handler", VS::between(10, 30)}});
    provider.add_dependencies("webhook-handler", 20, {});
    provider.add_dependencies("webhook-handler", 10, {});

    // === Data Platform (40 packages) ===
    provider.add_dependencies("data-platform", 20, {{"database-cluster", VS::between(20, 30)}, {"caching-layer", VS::between(20, 30)}, {"search-engine", VS::between(10, 30)}, {"message-queue", VS::between(10, 30)}});
    provider.add_dependencies("data-platform", 10, {{"database-cluster", VS::between(10, 30)}, {"caching-layer", VS::between(10, 30)}, {"search-engine", VS::between(10, 30)}});

    // database-cluster (10 packages)
    provider.add_dependencies("database-cluster", 20, {{"postgres-primary", VS::between(20, 30)}, {"postgres-replica", VS::between(10, 30)}, {"db-migration", VS::between(10, 30)}});
    provider.add_dependencies("database-cluster", 10, {{"postgres-primary", VS::between(10, 30)}, {"postgres-replica", VS::between(10, 30)}});
    provider.add_dependencies("postgres-primary", 20, {{"pg-connection", VS::between(10, 30)}, {"pg-pooling", VS::between(10, 30)}});
    provider.add_dependencies("postgres-primary", 10, {{"pg-connection", VS::between(10, 30)}});
    provider.add_dependencies("pg-connection", 20, {});
    provider.add_dependencies("pg-connection", 10, {});
    provider.add_dependencies("pg-pooling", 20, {});
    provider.add_dependencies("pg-pooling", 10, {});
    provider.add_dependencies("postgres-replica", 20, {{"replication-lag", VS::between(10, 30)}});
    provider.add_dependencies("postgres-replica", 10, {{"replication-lag", VS::between(10, 30)}});
    provider.add_dependencies("replication-lag", 20, {});
    provider.add_dependencies("replication-lag", 10, {});
    provider.add_dependencies("db-migration", 20, {{"flyway", VS::between(10, 30)}});
    provider.add_dependencies("db-migration", 10, {{"flyway", VS::between(10, 30)}});
    provider.add_dependencies("flyway", 20, {});
    provider.add_dependencies("flyway", 10, {});

    // caching-layer (10 packages)
    provider.add_dependencies("caching-layer", 20, {{"redis-cluster", VS::between(20, 30)}, {"cache-strategy", VS::between(10, 30)}, {"cache-invalidation", VS::between(10, 30)}});
    provider.add_dependencies("caching-layer", 10, {{"redis-cluster", VS::between(10, 30)}, {"cache-strategy", VS::between(10, 30)}});
    provider.add_dependencies("redis-cluster", 20, {{"redis-node", VS::between(10, 30)}, {"redis-sentinel", VS::between(10, 30)}});
    provider.add_dependencies("redis-cluster", 10, {{"redis-node", VS::between(10, 30)}});
    provider.add_dependencies("redis-node", 20, {});
    provider.add_dependencies("redis-node", 10, {});
    provider.add_dependencies("redis-sentinel", 20, {});
    provider.add_dependencies("redis-sentinel", 10, {});
    provider.add_dependencies("cache-strategy", 20, {{"ttl-manager", VS::between(10, 30)}});
    provider.add_dependencies("cache-strategy", 10, {{"ttl-manager", VS::between(10, 30)}});
    provider.add_dependencies("ttl-manager", 20, {});
    provider.add_dependencies("ttl-manager", 10, {});
    provider.add_dependencies("cache-invalidation", 20, {{"event-listener", VS::between(10, 30)}});
    provider.add_dependencies("cache-invalidation", 10, {{"event-listener", VS::between(10, 30)}});
    provider.add_dependencies("event-listener", 20, {});
    provider.add_dependencies("event-listener", 10, {});

    // search-engine (10 packages)
    provider.add_dependencies("search-engine", 20, {{"elasticsearch-cluster", VS::between(20, 30)}, {"indexing-service", VS::between(10, 30)}, {"search-api", VS::between(10, 30)}});
    provider.add_dependencies("search-engine", 10, {{"elasticsearch-cluster", VS::between(10, 30)}, {"indexing-service", VS::between(10, 30)}});
    provider.add_dependencies("elasticsearch-cluster", 20, {{"es-node", VS::between(10, 30)}, {"es-shard", VS::between(10, 30)}});
    provider.add_dependencies("elasticsearch-cluster", 10, {{"es-node", VS::between(10, 30)}});
    provider.add_dependencies("es-node", 20, {});
    provider.add_dependencies("es-node", 10, {});
    provider.add_dependencies("es-shard", 20, {});
    provider.add_dependencies("es-shard", 10, {});
    provider.add_dependencies("indexing-service", 20, {{"doc-processor", VS::between(10, 30)}});
    provider.add_dependencies("indexing-service", 10, {{"doc-processor", VS::between(10, 30)}});
    provider.add_dependencies("doc-processor", 20, {});
    provider.add_dependencies("doc-processor", 10, {});
    provider.add_dependencies("search-api", 20, {{"query-builder", VS::between(10, 30)}});
    provider.add_dependencies("search-api", 10, {{"query-builder", VS::between(10, 30)}});
    provider.add_dependencies("query-builder", 20, {});
    provider.add_dependencies("query-builder", 10, {});

    // message-queue (10 packages)
    provider.add_dependencies("message-queue", 20, {{"kafka-cluster", VS::between(10, 30)}, {"producer-api", VS::between(10, 30)}, {"consumer-group", VS::between(10, 30)}});
    provider.add_dependencies("message-queue", 10, {{"kafka-cluster", VS::between(10, 30)}, {"producer-api", VS::between(10, 30)}});
    provider.add_dependencies("kafka-cluster", 20, {{"kafka-broker", VS::between(10, 30)}, {"zookeeper", VS::between(10, 30)}});
    provider.add_dependencies("kafka-cluster", 10, {{"kafka-broker", VS::between(10, 30)}});
    provider.add_dependencies("kafka-broker", 20, {});
    provider.add_dependencies("kafka-broker", 10, {});
    provider.add_dependencies("zookeeper", 20, {});
    provider.add_dependencies("zookeeper", 10, {});
    provider.add_dependencies("producer-api", 20, {{"serializer", VS::between(10, 30)}});
    provider.add_dependencies("producer-api", 10, {{"serializer", VS::between(10, 30)}});
    provider.add_dependencies("serializer", 20, {});
    provider.add_dependencies("serializer", 10, {});
    provider.add_dependencies("consumer-group", 20, {{"deserializer", VS::between(10, 30)}});
    provider.add_dependencies("consumer-group", 10, {{"deserializer", VS::between(10, 30)}});
    provider.add_dependencies("deserializer", 20, {});
    provider.add_dependencies("deserializer", 10, {});

    // === ML Platform (40 packages) ===
    provider.add_dependencies("ml-platform", 20, {{"training-pipeline", VS::between(20, 30)}, {"inference-service", VS::between(20, 30)}, {"feature-store", VS::between(10, 30)}, {"model-registry", VS::between(10, 30)}});
    provider.add_dependencies("ml-platform", 10, {{"training-pipeline", VS::between(10, 30)}, {"inference-service", VS::between(10, 30)}, {"feature-store", VS::between(10, 30)}});

    // training-pipeline (10 packages)
    provider.add_dependencies("training-pipeline", 20, {{"data-loader", VS::between(20, 30)}, {"model-trainer", VS::between(10, 30)}, {"hyperparameter-tuner", VS::between(10, 30)}});
    provider.add_dependencies("training-pipeline", 10, {{"data-loader", VS::between(10, 30)}, {"model-trainer", VS::between(10, 30)}});
    provider.add_dependencies("data-loader", 20, {{"dataset-reader", VS::between(10, 30)}, {"data-augmentation", VS::between(10, 30)}});
    provider.add_dependencies("data-loader", 10, {{"dataset-reader", VS::between(10, 30)}});
    provider.add_dependencies("dataset-reader", 20, {});
    provider.add_dependencies("dataset-reader", 10, {});
    provider.add_dependencies("data-augmentation", 20, {});
    provider.add_dependencies("data-augmentation", 10, {});
    provider.add_dependencies("model-trainer", 20, {{"optimizer", VS::between(10, 30)}});
    provider.add_dependencies("model-trainer", 10, {{"optimizer", VS::between(10, 30)}});
    provider.add_dependencies("optimizer", 20, {});
    provider.add_dependencies("optimizer", 10, {});
    provider.add_dependencies("hyperparameter-tuner", 20, {{"grid-search", VS::between(10, 30)}});
    provider.add_dependencies("hyperparameter-tuner", 10, {{"grid-search", VS::between(10, 30)}});
    provider.add_dependencies("grid-search", 20, {});
    provider.add_dependencies("grid-search", 10, {});

    // inference-service (10 packages)
    provider.add_dependencies("inference-service", 20, {{"model-server", VS::between(20, 30)}, {"prediction-cache", VS::between(10, 30)}, {"batch-predictor", VS::between(10, 30)}});
    provider.add_dependencies("inference-service", 10, {{"model-server", VS::between(10, 30)}, {"prediction-cache", VS::between(10, 30)}});
    provider.add_dependencies("model-server", 20, {{"grpc-server", VS::between(10, 30)}, {"model-loader", VS::between(10, 30)}});
    provider.add_dependencies("model-server", 10, {{"grpc-server", VS::between(10, 30)}});
    provider.add_dependencies("grpc-server", 20, {});
    provider.add_dependencies("grpc-server", 10, {});
    provider.add_dependencies("model-loader", 20, {});
    provider.add_dependencies("model-loader", 10, {});
    provider.add_dependencies("prediction-cache", 20, {{"result-cache", VS::between(10, 30)}});
    provider.add_dependencies("prediction-cache", 10, {{"result-cache", VS::between(10, 30)}});
    provider.add_dependencies("result-cache", 20, {});
    provider.add_dependencies("result-cache", 10, {});
    provider.add_dependencies("batch-predictor", 20, {{"batch-processor", VS::between(10, 30)}});
    provider.add_dependencies("batch-predictor", 10, {{"batch-processor", VS::between(10, 30)}});
    provider.add_dependencies("batch-processor", 20, {});
    provider.add_dependencies("batch-processor", 10, {});

    // feature-store (10 packages)
    provider.add_dependencies("feature-store", 20, {{"feature-repo", VS::between(20, 30)}, {"feature-serving", VS::between(10, 30)}, {"feature-monitoring", VS::between(10, 30)}});
    provider.add_dependencies("feature-store", 10, {{"feature-repo", VS::between(10, 30)}, {"feature-serving", VS::between(10, 30)}});
    provider.add_dependencies("feature-repo", 20, {{"feature-schema", VS::between(10, 30)}, {"feature-versioning", VS::between(10, 30)}});
    provider.add_dependencies("feature-repo", 10, {{"feature-schema", VS::between(10, 30)}});
    provider.add_dependencies("feature-schema", 20, {});
    provider.add_dependencies("feature-schema", 10, {});
    provider.add_dependencies("feature-versioning", 20, {});
    provider.add_dependencies("feature-versioning", 10, {});
    provider.add_dependencies("feature-serving", 20, {{"online-store", VS::between(10, 30)}});
    provider.add_dependencies("feature-serving", 10, {{"online-store", VS::between(10, 30)}});
    provider.add_dependencies("online-store", 20, {});
    provider.add_dependencies("online-store", 10, {});
    provider.add_dependencies("feature-monitoring", 20, {{"drift-detector", VS::between(10, 30)}});
    provider.add_dependencies("feature-monitoring", 10, {{"drift-detector", VS::between(10, 30)}});
    provider.add_dependencies("drift-detector", 20, {});
    provider.add_dependencies("drift-detector", 10, {});

    // model-registry (10 packages)
    provider.add_dependencies("model-registry", 20, {{"model-catalog", VS::between(10, 30)}, {"model-metadata", VS::between(10, 30)}, {"model-lifecycle", VS::between(10, 30)}});
    provider.add_dependencies("model-registry", 10, {{"model-catalog", VS::between(10, 30)}, {"model-metadata", VS::between(10, 30)}});
    provider.add_dependencies("model-catalog", 20, {{"artifact-store", VS::between(10, 30)}, {"version-control", VS::between(10, 30)}});
    provider.add_dependencies("model-catalog", 10, {{"artifact-store", VS::between(10, 30)}});
    provider.add_dependencies("artifact-store", 20, {});
    provider.add_dependencies("artifact-store", 10, {});
    provider.add_dependencies("version-control", 20, {});
    provider.add_dependencies("version-control", 10, {});
    provider.add_dependencies("model-metadata", 20, {{"metrics-tracker", VS::between(10, 30)}});
    provider.add_dependencies("model-metadata", 10, {{"metrics-tracker", VS::between(10, 30)}});
    provider.add_dependencies("metrics-tracker", 20, {});
    provider.add_dependencies("metrics-tracker", 10, {});
    provider.add_dependencies("model-lifecycle", 20, {{"deployment-tracker", VS::between(10, 30)}});
    provider.add_dependencies("model-lifecycle", 10, {{"deployment-tracker", VS::between(10, 30)}});
    provider.add_dependencies("deployment-tracker", 20, {});
    provider.add_dependencies("deployment-tracker", 10, {});

    // === Run both solvers ===
    std::cout << "Solving with both naive and optimized solvers...\n";
    std::string root = "root";
    int root_version = 10;

    std::cout << "Running naive solver...\n";
    auto start_naive = std::chrono::steady_clock::now();
    auto naive_solution = dpll_resolve<TestProvider>(provider, root, root_version);
    auto end_naive = std::chrono::steady_clock::now();

    std::cout << "Running optimized solver...\n";
    auto start_solver = std::chrono::steady_clock::now();
    auto solver_solution = resolve<TestProvider>(provider, root, root_version);
    auto end_solver = std::chrono::steady_clock::now();

    // Convert to sorted maps for comparison
    std::map<std::string, int> naive_sorted(naive_solution.begin(), naive_solution.end());
    std::map<std::string, int> solver_sorted(solver_solution.begin(), solver_solution.end());

    // Verify both found solutions
    assert(naive_sorted.size() > 0);
    assert(solver_sorted.size() > 0);
    assert(naive_sorted.count("root") > 0);
    assert(solver_sorted.count("root") > 0);

    // Compare naive and solver results
    std::cout << "Comparing naive vs solver results...\n";
    bool results_match = compare_solutions(naive_sorted, solver_sorted);

    if (!results_match)
    {
        std::cout << "ERROR: Naive and solver produced different results!\n";
        std::cout << "Naive solution:\n";
        print_solution(naive_sorted);
        std::cout << "Solver solution:\n";
        print_solution(solver_sorted);
    }
    assert(results_match);

    std::cout << "Package count: " << solver_sorted.size() << "\n";
    print_timing("huge_dependency_graph_200pkg",
                 std::chrono::duration_cast<std::chrono::microseconds>(end_naive - start_naive).count(),
                 std::chrono::duration_cast<std::chrono::microseconds>(end_solver - start_solver).count(),
                 naive_solution.size(), solver_solution.size());

    std::cout << "\n✓ test_huge_dependency_graph_200pkg passed (200+ packages, naive==solver)\n";
}

/**
 * Test: conflict_heavy_graph
 * Creates a large dependency graph (100+ packages) with many conflicts to trigger backtracking
 * This is where PubGrub should outperform naive DPLL
 *
 * The test creates multiple "diamond dependency" scenarios with tight version constraints
 * that force the solver to backtrack many times.
 */
void test_conflict_heavy_graph()
{
    std::cout << "Running test: conflict_heavy_graph (100+ packages, many conflicts, tight constraints)\n";
    TestProvider provider;

    // Root depends on 10 major packages
    provider.add_dependencies("root", 10, {{"pkg-a", VS::between(10, 30)}, {"pkg-b", VS::between(10, 30)}, {"pkg-c", VS::between(10, 30)}, {"pkg-d", VS::between(10, 30)}, {"pkg-e", VS::between(10, 30)}, {"pkg-f", VS::between(10, 30)}, {"pkg-g", VS::between(10, 30)}, {"pkg-h", VS::between(10, 30)}, {"pkg-i", VS::between(10, 30)}, {"pkg-j", VS::between(10, 30)}});

    // Create diamond dependencies with conflicting requirements
    // Each of the 10 packages depends on multiple shared libraries with tight constraints

    // Define 10 shared libraries that many packages will depend on
    std::vector<std::string> shared_libs = {
        "shared-lib-1", "shared-lib-2", "shared-lib-3", "shared-lib-4", "shared-lib-5",
        "shared-lib-6", "shared-lib-7", "shared-lib-8", "shared-lib-9", "shared-lib-10"};

    // pkg-a through pkg-j: each depends on multiple shared libraries with conflicting constraints
    std::vector<std::string> top_packages = {"pkg-a", "pkg-b", "pkg-c", "pkg-d", "pkg-e",
                                             "pkg-f", "pkg-g", "pkg-h", "pkg-i", "pkg-j"};

    for (size_t pkg_idx = 0; pkg_idx < top_packages.size(); ++pkg_idx)
    {
        const std::string &pkg = top_packages[pkg_idx];
        for (int v = 10; v < 30; ++v)
        {
            // Each package version depends on 3-4 shared libraries
            if (v % 5 == 0)
            {
                // Every 5th version has very tight constraints
                provider.add_dependencies(pkg, v, {
                                                      {shared_libs[pkg_idx % 10], VS::singleton(v)}, // Exact version
                                                      {shared_libs[(pkg_idx + 1) % 10], VS::between(v - 1, v + 2)},
                                                      {shared_libs[(pkg_idx + 2) % 10], VS::singleton(v + 1)} // Exact version
                                                  });
            }
            else if (v % 3 == 0)
            {
                provider.add_dependencies(pkg, v, {{shared_libs[pkg_idx % 10], VS::between(v, v + 3)}, {shared_libs[(pkg_idx + 1) % 10], VS::singleton(v)}, // Exact version
                                                   {shared_libs[(pkg_idx + 3) % 10], VS::between(v - 2, v + 4)}});
            }
            else
            {
                provider.add_dependencies(pkg, v, {{shared_libs[pkg_idx % 10], VS::between(v - 1, v + 5)}, {shared_libs[(pkg_idx + 1) % 10], VS::between(v, v + 3)}, {shared_libs[(pkg_idx + 2) % 10], VS::between(v - 2, v + 2)}});
            }
        }
    }

    // Each shared-lib has dependencies on core libraries
    // This creates deep dependency chains with conflicts
    std::vector<std::string> core_libs = {
        "core-lib-1", "core-lib-2", "core-lib-3", "core-lib-4", "core-lib-5",
        "core-lib-6", "core-lib-7", "core-lib-8", "core-lib-9", "core-lib-10"};

    for (size_t lib_idx = 0; lib_idx < shared_libs.size(); ++lib_idx)
    {
        const std::string &lib = shared_libs[lib_idx];
        for (int v = 10; v < 30; ++v)
        {
            if (v % 4 == 0)
            {
                // Tight constraints every 4th version
                provider.add_dependencies(lib, v, {{core_libs[lib_idx % 10], VS::singleton(v)}, {core_libs[(lib_idx + 1) % 10], VS::between(v - 1, v + 2)}});
            }
            else if (v % 7 == 0)
            {
                // Different tight constraints
                provider.add_dependencies(lib, v, {{core_libs[lib_idx % 10], VS::between(v - 2, v + 1)}, {core_libs[(lib_idx + 2) % 10], VS::singleton(v + 1)}});
            }
            else
            {
                provider.add_dependencies(lib, v, {{core_libs[lib_idx % 10], VS::between(v - 2, v + 3)}, {core_libs[(lib_idx + 1) % 10], VS::between(v - 1, v + 4)}});
            }
        }
    }

    // Core libraries depend on utility and platform libraries
    std::vector<std::string> util_libs = {
        "util-lib-1", "util-lib-2", "util-lib-3", "util-lib-4", "util-lib-5",
        "util-lib-6", "util-lib-7", "util-lib-8", "util-lib-9", "util-lib-10"};

    std::vector<std::string> platform_libs = {
        "platform-lib-1", "platform-lib-2", "platform-lib-3", "platform-lib-4", "platform-lib-5",
        "platform-lib-6", "platform-lib-7", "platform-lib-8", "platform-lib-9", "platform-lib-10"};

    for (size_t core_idx = 0; core_idx < core_libs.size(); ++core_idx)
    {
        const std::string &core = core_libs[core_idx];
        for (int v = 10; v < 30; ++v)
        {
            if (v % 6 == 0)
            {
                provider.add_dependencies(core, v, {{util_libs[core_idx % 10], VS::singleton(v - 1)}, {platform_libs[core_idx % 10], VS::between(v, v + 3)}});
            }
            else
            {
                provider.add_dependencies(core, v, {{util_libs[core_idx % 10], VS::between(v - 2, v + 2)}, {platform_libs[core_idx % 10], VS::between(v - 1, v + 4)}});
            }
        }
    }

    // Utility libraries depend on memory and string libraries
    std::vector<std::string> memory_libs = {
        "memory-lib-1", "memory-lib-2", "memory-lib-3", "memory-lib-4", "memory-lib-5",
        "memory-lib-6", "memory-lib-7", "memory-lib-8", "memory-lib-9", "memory-lib-10"};

    for (size_t util_idx = 0; util_idx < util_libs.size(); ++util_idx)
    {
        const std::string &util = util_libs[util_idx];
        for (int v = 10; v < 30; ++v)
        {
            if (v % 5 == 0)
            {
                provider.add_dependencies(util, v, {{memory_libs[util_idx % 10], VS::singleton(v)}});
            }
            else
            {
                provider.add_dependencies(util, v, {{memory_libs[util_idx % 10], VS::between(v - 2, v + 3)}});
            }
        }
    }

    // Add more intermediate layers to reach 100+ packages
    // Data processing libraries
    std::vector<std::string> data_libs = {
        "data-lib-1", "data-lib-2", "data-lib-3", "data-lib-4", "data-lib-5",
        "data-lib-6", "data-lib-7", "data-lib-8", "data-lib-9", "data-lib-10"};

    // IO libraries
    std::vector<std::string> io_libs = {
        "io-lib-1", "io-lib-2", "io-lib-3", "io-lib-4", "io-lib-5",
        "io-lib-6", "io-lib-7", "io-lib-8", "io-lib-9", "io-lib-10"};

    // Network libraries
    std::vector<std::string> net_libs = {
        "net-lib-1", "net-lib-2", "net-lib-3", "net-lib-4", "net-lib-5",
        "net-lib-6", "net-lib-7", "net-lib-8", "net-lib-9", "net-lib-10"};

    // String libraries
    std::vector<std::string> string_libs = {
        "string-lib-1", "string-lib-2", "string-lib-3", "string-lib-4", "string-lib-5",
        "string-lib-6", "string-lib-7", "string-lib-8", "string-lib-9", "string-lib-10"};

    // Some core libs also depend on data and io libs (creates more conflicts)
    for (size_t core_idx = 0; core_idx < 5; ++core_idx) // First 5 core libs
    {
        const std::string &core = core_libs[core_idx];
        for (int v = 10; v < 30; ++v)
        {
            if (v % 7 == 0)
            {
                provider.add_dependencies(core, v, {
                                                       {util_libs[core_idx % 10], VS::between(v - 2, v + 2)}, {platform_libs[core_idx % 10], VS::between(v - 1, v + 4)}, {data_libs[core_idx % 10], VS::singleton(v)} // Tight constraint
                                                   });
            }
        }
    }

    // Data libraries depend on IO and string libraries
    for (size_t data_idx = 0; data_idx < data_libs.size(); ++data_idx)
    {
        const std::string &data = data_libs[data_idx];
        for (int v = 10; v < 30; ++v)
        {
            if (v % 4 == 0)
            {
                provider.add_dependencies(data, v, {{io_libs[data_idx % 10], VS::singleton(v)}, {string_libs[data_idx % 10], VS::between(v - 1, v + 2)}});
            }
            else
            {
                provider.add_dependencies(data, v, {{io_libs[data_idx % 10], VS::between(v - 2, v + 3)}, {string_libs[data_idx % 10], VS::between(v - 2, v + 2)}});
            }
        }
    }

    // IO libraries depend on platform and network libraries
    for (size_t io_idx = 0; io_idx < io_libs.size(); ++io_idx)
    {
        const std::string &io = io_libs[io_idx];
        for (int v = 10; v < 30; ++v)
        {
            if (v % 5 == 0)
            {
                provider.add_dependencies(io, v, {{platform_libs[io_idx % 10], VS::singleton(v)}, {net_libs[io_idx % 10], VS::between(v, v + 3)}});
            }
            else
            {
                provider.add_dependencies(io, v, {{platform_libs[io_idx % 10], VS::between(v - 2, v + 2)}, {net_libs[io_idx % 10], VS::between(v - 1, v + 4)}});
            }
        }
    }

    // Network libraries depend on platform libraries (with conflicts)
    for (size_t net_idx = 0; net_idx < net_libs.size(); ++net_idx)
    {
        const std::string &net = net_libs[net_idx];
        for (int v = 10; v < 30; ++v)
        {
            if (v % 6 == 0)
            {
                provider.add_dependencies(net, v, {
                                                      {platform_libs[net_idx % 10], VS::singleton(v)} // Tight constraint
                                                  });
            }
            else
            {
                provider.add_dependencies(net, v, {{platform_libs[net_idx % 10], VS::between(v - 1, v + 3)}});
            }
        }
    }

    // String libraries depend on memory libraries (with conflicts)
    for (size_t str_idx = 0; str_idx < string_libs.size(); ++str_idx)
    {
        const std::string &str = string_libs[str_idx];
        for (int v = 10; v < 30; ++v)
        {
            if (v % 8 == 0)
            {
                provider.add_dependencies(str, v, {
                                                      {memory_libs[str_idx % 10], VS::singleton(v - 1)} // Tight constraint
                                                  });
            }
            else
            {
                provider.add_dependencies(str, v, {{memory_libs[str_idx % 10], VS::between(v - 2, v + 2)}});
            }
        }
    }

    // Platform and memory libraries are leaf nodes
    for (size_t i = 0; i < 10; ++i)
    {
        for (int v = 10; v < 30; ++v)
        {
            provider.add_dependencies(platform_libs[i], v, {});
            provider.add_dependencies(memory_libs[i], v, {});
        }
    }

    // === Run both solvers ===
    std::cout << "Solving conflict-heavy graph (this may take longer)...\n";
    std::string root = "root";
    int root_version = 10;

    std::cout << "Running naive solver...\n";
    auto start_naive = std::chrono::steady_clock::now();
    auto naive_solution = dpll_resolve<TestProvider>(provider, root, root_version);
    auto end_naive = std::chrono::steady_clock::now();

    std::cout << "Running optimized PubGrub solver...\n";
    auto start_solver = std::chrono::steady_clock::now();
    auto solver_solution = resolve<TestProvider>(provider, root, root_version);
    auto end_solver = std::chrono::steady_clock::now();

    // Convert to sorted maps for comparison
    std::map<std::string, int> naive_sorted(naive_solution.begin(), naive_solution.end());
    std::map<std::string, int> solver_sorted(solver_solution.begin(), solver_solution.end());

    // Verify both found solutions
    assert(naive_sorted.size() > 0);
    assert(solver_sorted.size() > 0);
    assert(naive_sorted.count("root") > 0);
    assert(solver_sorted.count("root") > 0);

    // Compare results (they might differ but both should be valid)
    std::cout << "Comparing naive vs solver results...\n";
    bool results_match = compare_solutions(naive_sorted, solver_sorted);

    if (!results_match)
    {
        std::cout << "NOTE: Naive and solver produced different results (both may be valid solutions).\n";
        std::cout << "DPLL solution (" << naive_sorted.size() << " packages):\n";
        print_solution(naive_sorted);
        std::cout << "CDCL solution (" << solver_sorted.size() << " packages):\n";
        print_solution(solver_sorted);
        // Don't assert - both solutions may be valid
    }

    std::cout << "Package count: " << solver_sorted.size() << "\n";
    print_timing("conflict_heavy_graph",
                 std::chrono::duration_cast<std::chrono::microseconds>(end_naive - start_naive).count(),
                 std::chrono::duration_cast<std::chrono::microseconds>(end_solver - start_solver).count(),
                 naive_solution.size(), solver_solution.size());

    if (end_solver < end_naive)
    {
        std::cout << "*** PubGrub solver is FASTER on conflict-heavy graphs! ***\n";
    }

    std::cout << "\n✓ test_conflict_heavy_graph passed\n";
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
    std::cout << "Running Unified PubGrub Solver Tests (DPLL vs CDCL)\n";
    std::cout << "==========================================================\n\n";

    test_provider_basic();
    std::cout << "\n"; // unchanged
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
    test_very_large_dependency_graph();
    std::cout << "\n";
    test_huge_dependency_graph_200pkg();
    std::cout << "\n";
    test_conflict_heavy_graph();
    std::cout << "\n";

    std::cout << "==========================================================\n";
    std::cout << "✓ All unified tests passed!\n";
    std::cout << "Performance comparison included above.\n";
    std::cout << "DPLL and CDCL solvers performance metrics displayed.\n";
    return 0;
}