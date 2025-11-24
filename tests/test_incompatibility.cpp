#include "../include/incompatibility.h"
#include "../include/term.h"
#include "../include/ranges.h"
#include "../include/arena.h"
#include <cassert>
#include <iostream>
#include <string>

// Define simple types for testing
using Package = std::string;
using Version = int;
using Metadata = std::string;
using PkgId = Id<Package>;
using Incompat = Incompatibility<Package, Version, Metadata>;

void test_incompatibility_not_root()
{
    // Create package arena
    HashArena<Package> packages;
    auto pkg_id = packages.alloc("root");

    // Create not_root incompatibility
    auto incomp = Incompat::not_root(pkg_id, 1);

    // Verify it has one term
    assert(incomp.size() == 1);

    // Verify the term is negative for version 1
    auto* term = incomp.get(pkg_id);
    assert(term != nullptr);
    assert(term->is_negative());
    assert(!term->contains(1));  // Negative of {1} should not contain 1
    assert(term->contains(0));   // But should contain other versions
    assert(term->contains(2));

    std::cout << "✓ test_incompatibility_not_root passed\n";
}

void test_incompatibility_no_versions()
{
    HashArena<Package> packages;
    auto pkg_id = packages.alloc("foo");

    // Create a positive term for versions >= 10
    auto ranges = Ranges<Version>::higher_than(10);
    auto term = Term<Version>::Positive(ranges);

    // Create no_versions incompatibility
    auto incomp = Incompat::no_versions(pkg_id, term);

    assert(incomp.size() == 1);
    auto* t = incomp.get(pkg_id);
    assert(t != nullptr);
    assert(t->is_positive());

    std::cout << "✓ test_incompatibility_no_versions passed\n";
}

void test_incompatibility_custom_term()
{
    HashArena<Package> packages;
    auto pkg_id = packages.alloc("bar");

    // Create a negative term
    auto ranges = Ranges<Version>::higher_than(5);
    auto term = Term<Version>::Negative(ranges);

    // Create custom incompatibility
    auto incomp = Incompat::custom_term(pkg_id, term, "custom metadata");

    assert(incomp.size() == 1);
    auto* t = incomp.get(pkg_id);
    assert(t != nullptr);
    assert(t->is_negative());

    std::cout << "✓ test_incompatibility_custom_term passed\n";
}

void test_incompatibility_from_dependency()
{
    HashArena<Package> packages;
    auto pkg1_id = packages.alloc("app");
    auto pkg2_id = packages.alloc("lib");

    // app [1,5) depends on lib [10,20)
    auto app_versions = Ranges<Version>::between(1, 5);
    auto lib_versions = Ranges<Version>::between(10, 20);

    auto incomp = Incompat::from_dependency(pkg1_id, app_versions,
                                            std::make_pair(pkg2_id, lib_versions));

    assert(incomp.size() == 2);

    // Check app term (positive)
    auto* app_term = incomp.get(pkg1_id);
    assert(app_term != nullptr);
    assert(app_term->is_positive());

    // Check lib term (negative - not in the range)
    auto* lib_term = incomp.get(pkg2_id);
    assert(lib_term != nullptr);
    assert(lib_term->is_negative());

    // Verify as_dependency
    auto dep = incomp.as_dependency();
    assert(dep.has_value());
    assert(dep->first == pkg1_id);
    assert(dep->second == pkg2_id);

    std::cout << "✓ test_incompatibility_from_dependency passed\n";
}

void test_incompatibility_merge_dependents()
{
    HashArena<Package> packages;
    auto pkg1_id = packages.alloc("app");
    auto pkg2_id = packages.alloc("lib");

    // Create two dependency incompatibilities
    // app [1,3) -> lib [10,20)
    auto ranges1 = Ranges<Version>::between(1, 3);
    auto lib_ranges = Ranges<Version>::between(10, 20);
    auto incomp1 = Incompat::from_dependency(pkg1_id, ranges1,
                                             std::make_pair(pkg2_id, lib_ranges));

    // app [3,5) -> lib [10,20) (same dependency, different version range)
    auto ranges2 = Ranges<Version>::between(3, 5);
    auto incomp2 = Incompat::from_dependency(pkg1_id, ranges2,
                                             std::make_pair(pkg2_id, lib_ranges));

    // Merge should combine version ranges for app
    auto merged = incomp1.merge_dependents(incomp2);
    assert(merged.has_value());
    assert(merged->size() == 2);

    std::cout << "✓ test_incompatibility_merge_dependents passed\n";
}

void test_incompatibility_size_and_iteration()
{
    HashArena<Package> packages;
    auto pkg1_id = packages.alloc("pkg1");
    auto pkg2_id = packages.alloc("pkg2");

    auto ranges1 = Ranges<Version>::higher_than(5);
    auto ranges2 = Ranges<Version>::lower_than(10);

    auto incomp = Incompat::from_dependency(pkg1_id, ranges1,
                                            std::make_pair(pkg2_id, ranges2));

    assert(incomp.size() == 2);

    // Test iteration
    int count = 0;
    for (auto it = incomp.begin(); it != incomp.end(); ++it)
    {
        count++;
        // Verify we can access the package ID and term
        assert(it->first == pkg1_id || it->first == pkg2_id);
    }
    assert(count == 2);

    std::cout << "✓ test_incompatibility_size_and_iteration passed\n";
}

int main()
{
    std::cout << "Running Incompatibility tests...\n\n";

    test_incompatibility_not_root();
    test_incompatibility_no_versions();
    test_incompatibility_custom_term();
    test_incompatibility_from_dependency();
    test_incompatibility_merge_dependents();
    test_incompatibility_size_and_iteration();

    std::cout << "\n✓ All Incompatibility tests passed!\n";
    return 0;
}
