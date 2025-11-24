#include "../include/provider.h"
#include "../include/ranges.h"
#include <cassert>
#include <iostream>
#include <string>

// Test PackageResolutionStatistics
void test_package_resolution_statistics()
{
    PackageResolutionStatistics stats;
    assert(stats.conflict_count() == 0);

    stats.unit_propagation_affected = 5;
    stats.unit_propagation_culprit = 3;
    stats.dependencies_affected = 2;
    stats.dependencies_culprit = 1;

    assert(stats.conflict_count() == 11);
    std::cout << "✓ test_package_resolution_statistics passed\n";
}

// Test DecisionLevel basic construction and equality
void test_decision_level_construction()
{
    DecisionLevel level0;
    assert(level0.level == 0);

    DecisionLevel level5(5);
    assert(level5.level == 5);

    assert(level0 == DecisionLevel(0));
    assert(level5 == DecisionLevel(5));

    std::cout << "✓ test_decision_level_construction passed\n";
}

// Test DecisionLevel increment
void test_decision_level_increment()
{
    DecisionLevel level0(0);
    DecisionLevel level1 = level0.increment();

    assert(level0.level == 0);
    assert(level1.level == 1);

    DecisionLevel level2 = level1.increment();
    assert(level2.level == 2);

    std::cout << "✓ test_decision_level_increment passed\n";
}

// Test DecisionLevel comparison operators
void test_decision_level_comparisons()
{
    DecisionLevel level0(0);
    DecisionLevel level1(1);
    DecisionLevel level2(2);
    DecisionLevel level1_copy(1);

    // Equality
    assert(level1 == level1_copy);

    // Less than
    assert(level0 < level1);
    assert(level1 < level2);
    assert(!(level1 < level0));
    assert(!(level1 < level1_copy));

    // Less than or equal
    assert(level0 <= level1);
    assert(level1 <= level2);
    assert(level1 <= level1_copy);
    assert(!(level2 <= level1));

    // Greater than
    assert(level2 > level1);
    assert(level1 > level0);
    assert(!(level0 > level1));
    assert(!(level1 > level1_copy));

    // Greater than or equal
    assert(level2 >= level1);
    assert(level1 >= level0);
    assert(level1 >= level1_copy);
    assert(!(level0 >= level1));

    std::cout << "✓ test_decision_level_comparisons passed\n";
}

// Test Dependencies structure
void test_dependencies_available()
{
    using Deps = Dependencies<std::string, int, std::string>;
    using VS = Ranges<int>;

    std::map<std::string, VS> deps_map;
    deps_map["package_a"] = Ranges<int>::higher_than(1);
    deps_map["package_b"] = Ranges<int>::lower_than(10);

    auto deps = Deps::available(deps_map, "test metadata");

    assert(deps.tag == Availability::Available);
    assert(deps.dependencies.size() == 2);
    assert(deps.dependencies.count("package_a") == 1);
    assert(deps.dependencies.count("package_b") == 1);
    assert(deps.meta == "test metadata");

    std::cout << "✓ test_dependencies_available passed\n";
}

void test_dependencies_unavailable()
{
    using Deps = Dependencies<std::string, int, std::string>;

    auto deps = Deps::unavailable("not found");

    assert(deps.tag == Availability::Unavailable);
    assert(deps.dependencies.empty());
    assert(deps.meta == "not found");

    std::cout << "✓ test_dependencies_unavailable passed\n";
}

// Test OfflineDependencyProvider
void test_offline_provider_add_dependencies()
{
    using Provider = OfflineDependencyProvider<std::string, int>;
    using VS = Ranges<int>;

    Provider provider;

    // Add dependencies using initializer_list
    provider.add_dependencies("root", 1, {{"dep_a", Ranges<int>::higher_than(0)}, {"dep_b", Ranges<int>::between(1, 5)}});

    auto packages = provider.packages();
    assert(packages.size() == 1);
    assert(packages[0] == "root");

    auto versions = provider.versions("root");
    assert(versions.has_value());
    assert(versions->size() == 1);
    assert((*versions)[0] == 1);

    auto deps = provider.dependencies("root", 1);
    assert(deps.has_value());
    assert(deps->size() == 2);
    assert(deps->count("dep_a") == 1);
    assert(deps->count("dep_b") == 1);

    std::cout << "✓ test_offline_provider_add_dependencies passed\n";
}

void test_offline_provider_multiple_versions()
{
    using Provider = OfflineDependencyProvider<std::string, int>;

    Provider provider;

    // Add multiple versions of the same package
    provider.add_dependencies("package", 1, {});
    provider.add_dependencies("package", 2, {});
    provider.add_dependencies("package", 3, {});

    auto versions = provider.versions("package");
    assert(versions.has_value());
    assert(versions->size() == 3);

    // Versions should be in order
    assert((*versions)[0] == 1);
    assert((*versions)[1] == 2);
    assert((*versions)[2] == 3);

    std::cout << "✓ test_offline_provider_multiple_versions passed\n";
}

void test_offline_provider_nonexistent_package()
{
    using Provider = OfflineDependencyProvider<std::string, int>;

    Provider provider;

    auto versions = provider.versions("nonexistent");
    assert(!versions.has_value());

    auto deps = provider.dependencies("nonexistent", 1);
    assert(!deps.has_value());

    std::cout << "✓ test_offline_provider_nonexistent_package passed\n";
}

void test_offline_provider_choose_version()
{
    using Provider = OfflineDependencyProvider<std::string, int>;
    using VS = Ranges<int>;

    Provider provider;

    // Add versions 1, 2, 5, 10 for a package
    provider.add_dependencies("package", 1, {});
    provider.add_dependencies("package", 2, {});
    provider.add_dependencies("package", 5, {});
    provider.add_dependencies("package", 10, {});

    // choose_version should pick the highest version in the range
    auto range_all = Ranges<int>::higher_than(0);
    auto chosen = provider.choose_version("package", range_all);
    assert(chosen.has_value());
    assert(*chosen == 10);

    // Range that only includes 1, 2, 5
    auto range_low = Ranges<int>::between(0, 8);
    auto chosen_low = provider.choose_version("package", range_low);
    assert(chosen_low.has_value());
    assert(*chosen_low == 5);

    // Range that includes nothing
    auto range_empty = Ranges<int>::between(11, 20);
    auto chosen_empty = provider.choose_version("package", range_empty);
    assert(!chosen_empty.has_value());

    // Nonexistent package
    auto chosen_none = provider.choose_version("nonexistent", range_all);
    assert(!chosen_none.has_value());

    std::cout << "✓ test_offline_provider_choose_version passed\n";
}

void test_offline_provider_get_dependencies()
{
    using Provider = OfflineDependencyProvider<std::string, int>;
    using VS = Ranges<int>;

    Provider provider;

    // Add package with dependencies
    provider.add_dependencies("package", 1, {{"dep1", Ranges<int>::higher_than(0)}, {"dep2", Ranges<int>::lower_than(10)}});

    // Get dependencies for existing package and version
    auto deps = provider.get_dependencies("package", 1);
    assert(deps.tag == Availability::Available);
    assert(deps.dependencies.size() == 2);
    assert(deps.dependencies.count("dep1") == 1);
    assert(deps.dependencies.count("dep2") == 1);
    assert(deps.meta == "OK");

    // Get dependencies for nonexistent package
    auto deps_no_pkg = provider.get_dependencies("nonexistent", 1);
    assert(deps_no_pkg.tag == Availability::Unavailable);
    assert(deps_no_pkg.dependencies.empty());
    assert(deps_no_pkg.meta == "Package not found");

    // Get dependencies for nonexistent version
    auto deps_no_ver = provider.get_dependencies("package", 99);
    assert(deps_no_ver.tag == Availability::Unavailable);
    assert(deps_no_ver.dependencies.empty());
    assert(deps_no_ver.meta == "Version not found");

    std::cout << "✓ test_offline_provider_get_dependencies passed\n";
}

void test_offline_provider_prioritize()
{
    using Provider = OfflineDependencyProvider<std::string, int>;
    using VS = Ranges<int>;

    Provider provider;

    // Add package with multiple versions
    provider.add_dependencies("package", 1, {});
    provider.add_dependencies("package", 2, {});
    provider.add_dependencies("package", 5, {});

    PackageResolutionStatistics stats;
    stats.unit_propagation_affected = 2;
    stats.dependencies_affected = 3;

    // Note: There's a typo in provider.h line 180: "Prioirty" instead of "Priority"
    // This test assumes the typo is fixed or we work around it

    auto range = Ranges<int>::higher_than(0);
    auto priority = provider.prioritize("package", range, stats);

    // Priority should be (conflict_count, version_count)
    assert(priority.first == 5);   // conflict_count = 2 + 3
    assert(priority.second == -3); // 3 versions in range

    // Test with limited range
    auto range_limited = Ranges<int>::between(0, 3);
    auto priority_limited = provider.prioritize("package", range_limited, stats);
    assert(priority_limited.first == 5);
    assert(priority_limited.second == -2); // only versions 1 and 2

    // Test with nonexistent package
    auto priority_none = provider.prioritize("nonexistent", range, stats);
    assert(priority_none.first == std::numeric_limits<std::uint32_t>::max());
    assert(priority_none.second == 0);

    std::cout << "✓ test_offline_provider_prioritize passed\n";
}

void test_offline_provider_complex_scenario()
{
    using Provider = OfflineDependencyProvider<std::string, int>;
    using VS = Ranges<int>;

    Provider provider;

    // Build a small dependency graph
    // root@3 depends on A^1 and B^2
    provider.add_dependencies("root", 3, {{"A", Ranges<int>::higher_than(1)}, {"B", Ranges<int>::higher_than(2)}});

    // A@1 has no dependencies
    // A@2 depends on C^5
    provider.add_dependencies("A", 1, {});
    provider.add_dependencies("A", 2, {{"C", Ranges<int>::higher_than(5)}});

    // B@2 depends on C^3
    // B@3 depends on C^4
    provider.add_dependencies("B", 2, {{"C", Ranges<int>::higher_than(3)}});
    provider.add_dependencies("B", 3, {{"C", Ranges<int>::higher_than(4)}});

    // C has versions 1 through 10
    for (int i = 1; i <= 10; ++i)
    {
        provider.add_dependencies("C", i, {});
    }

    // Verify package list
    auto packages = provider.packages();
    assert(packages.size() == 4);

    // Verify we can choose appropriate versions
    auto chosen_a = provider.choose_version("A", Ranges<int>::higher_than(1));
    assert(chosen_a.has_value() && *chosen_a == 2);

    auto chosen_b = provider.choose_version("B", Ranges<int>::higher_than(2));
    assert(chosen_b.has_value() && *chosen_b == 3);

    auto chosen_c = provider.choose_version("C", Ranges<int>::higher_than(5));
    assert(chosen_c.has_value() && *chosen_c == 10);

    // Verify dependency chains
    auto root_deps = provider.get_dependencies("root", 3);
    assert(root_deps.tag == Availability::Available);
    assert(root_deps.dependencies.size() == 2);

    auto a2_deps = provider.get_dependencies("A", 2);
    assert(a2_deps.tag == Availability::Available);
    assert(a2_deps.dependencies.size() == 1);
    assert(a2_deps.dependencies.count("C") == 1);

    std::cout << "✓ test_offline_provider_complex_scenario passed\n";
}

int main()
{
    std::cout << "Running Provider tests...\n\n";

    // PackageResolutionStatistics tests
    test_package_resolution_statistics();

    // DecisionLevel tests
    test_decision_level_construction();
    test_decision_level_increment();
    test_decision_level_comparisons();

    // Dependencies tests
    test_dependencies_available();
    test_dependencies_unavailable();

    // OfflineDependencyProvider tests
    test_offline_provider_add_dependencies();
    test_offline_provider_multiple_versions();
    test_offline_provider_nonexistent_package();
    test_offline_provider_choose_version();
    test_offline_provider_get_dependencies();
    test_offline_provider_prioritize();
    test_offline_provider_complex_scenario();

    std::cout << "\n✓ All Provider tests passed!\n";
    return 0;
}
