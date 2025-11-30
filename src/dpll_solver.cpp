#include "../include/provider.h"
#include "../include/incompatibility.h"
#include "../include/core.h"
using namespace std;

template <class P, class V>
class DPLLPackageAssignments
{
    using VS = Ranges<V>;
    std::pair<Id<P>, VS> assignment;

public:
    void setAssignment(const Id<P> &id, const VS &vs)
    {
        assignment = std::make_pair(id, vs);
    }

    std::pair<Id<P>, VS> getAssignment() const
    {
        return assignment;
    }
};

template <class P, class V>
class DPLLPartialSolution
{
    using VS = Ranges<V>;
    std::unordered_map<Id<P>, DPLLPackageAssignments<P, V>> assignments_map;

public:
    void addAssignment(const Id<P> &id, const VS &vs)
    {
        assignments_map[id].setAssignment(id, vs);
    }
    void deleteAssignment(const Id<P> &id)
    {
        assignments_map.erase(id);
    }
    std::optional<DPLLPackageAssignments<P, V>> getAssignment(const Id<P> &id) const
    {
        auto it = assignments_map.find(id);
        if (it != assignments_map.end())
        {
            return it->second;
        }
        return std::nullopt;
    }
    const std::unordered_map<Id<P>, DPLLPackageAssignments<P, V>> &getAssignments() const
    {
        return assignments_map;
    }

    void show(HashArena<P> &package_store) const
    {
        std::cout << "Showing current assignments:\n";
        for (const auto &[id, assignment] : assignments_map)
        {
            std::cout << "Package ID: " << package_store[id] << ", Assignment: " << assignment.getAssignment().second << std::endl;
        }
    }
};

template <typename DP>
bool dpll_resolve(
    OfflineDependencyProvider<typename DP::P, typename DP::V> &dependency_provider,
    DPLLPartialSolution<typename DP::P, typename DP::V> &partial_solution,
    const typename DP::P &package,
    HashArena<typename DP::P> &package_store,
    std::unordered_map<typename DP::P, Ranges<typename DP::V>> &not_completed_dependencies,
    std::vector<Id<typename DP::P>> &next_packages)
{
    using Types = PubGrubTypes<DP>;
    using P = typename Types::P;
    using V = typename Types::V;
    using M = typename Types::M;
    using VS = typename Types::VS;
    using DependencyConstraints = typename Types::DependencyConstraints;
    std::vector<Id<P>> new_next_packages;

    // Make a local copy of package to avoid reference invalidation
    P package_copy = package;

    auto available_versions_opt = dependency_provider.versions(package_copy);
    if (!available_versions_opt.has_value() || available_versions_opt->empty())
    {
        throw std::runtime_error("No versions available for package");
    }

    auto available_versions = *available_versions_opt;
    // Try versions from highest to lowest
    std::sort(available_versions.begin(), available_versions.end(), std::greater<V>());

    for (const V &ver : available_versions)
    {
        // Save current state before trying this version
        std::unordered_map<P, Ranges<V>> saved_not_completed = not_completed_dependencies;
        // Get dependencies for this version
        auto deps_result = dependency_provider.get_dependencies(package_copy, ver);
        if (deps_result.tag != Availability::Available)
        {
            continue;
        }

        bool can_assign = true;
        new_next_packages.clear();
        // Check each dependency
        for (const auto &[dep_pkg, dep_range] : deps_result.dependencies)
        {
            Id<P> dep_pkg_id = package_store.alloc(dep_pkg);

            if (const auto assignment = partial_solution.getAssignment(dep_pkg_id); assignment.has_value())
            {
                // Check if the assigned version satisfies the dependency
                const VS &assigned_range = assignment->getAssignment().second;
                // assigned_range is a singleton, extract the version
                if (!assigned_range.segments().empty() && assigned_range.segments().front().first.is_finite())
                {
                    V assigned_version = assigned_range.segments().front().first.value;

                    if (!dep_range.contains(assigned_version))
                    {
                        can_assign = false;
                        break;
                    }
                }
            }
            else
            {
                // No assignment yet, will need to process this package
                new_next_packages.push_back(dep_pkg_id);
                if (not_completed_dependencies.find(dep_pkg) != not_completed_dependencies.end())
                {
                    not_completed_dependencies[dep_pkg] = not_completed_dependencies[dep_pkg].intersection(dep_range);
                }
                else
                {
                    not_completed_dependencies.insert({dep_pkg, dep_range});
                }
            }
        }

        auto not_completed_it = not_completed_dependencies.find(package_copy);
        if (not_completed_it != not_completed_dependencies.end())
        {
            if (!not_completed_it->second.contains(ver))
            {
                can_assign = false;
                continue;
            }
        }

        if (can_assign)
        {
            // Assign this package first
            Id<P> pkg_id = package_store.alloc(package_copy);
            partial_solution.addAssignment(pkg_id, Ranges<V>::singleton(ver));

            for (const auto &pkg : new_next_packages)
                next_packages.push_back(pkg);

            // If there are dependencies to process
            bool all_deps_satisfied = true;
            while (!next_packages.empty())
            {
                // Process next dependency
                Id<P> next_pkg = next_packages.back();
                next_packages.pop_back();
                // Make a copy to avoid reference invalidation when package_store is modified
                P next_package = package_store[next_pkg];

                bool result = dpll_resolve<DP>(
                    dependency_provider, partial_solution, next_package, package_store, not_completed_dependencies, next_packages);
                if (!result)
                {
                    partial_solution.deleteAssignment(pkg_id);
                    // Restore the saved state before trying this version
                    not_completed_dependencies = saved_not_completed;
                    all_deps_satisfied = false;
                    break; // Try next version
                }
            }

            if (all_deps_satisfied)
            {
                return true;
            }
        }
        else
        {
            not_completed_dependencies = saved_not_completed;
        }
    }
    return false;
}

template <typename DP>
std::unordered_map<typename DP::P, typename DP::V> dpll_resolve(
    OfflineDependencyProvider<typename DP::P, typename DP::V> &dependency_provider,
    const typename DP::P &package,
    const typename DP::V &version)
{
    using P = typename DP::P;
    using V = typename DP::V;
    using M = typename DP::M;
    using VS = Ranges<V>;
    using DependencyConstraints = std::map<P, VS>;
    using Priority = typename DP::Priority;
    using IncompId = Id<Incompatibility<P, V, M>>;
    using Incomp = Incompatibility<P, V, M>;

    // Initialize state with root package and version
    State<DP> state = State<DP>::init(package, version);
    std::map<Id<P>, PackageResolutionStatistics> conflict_tracker;
    std::map<Id<P>, std::set<V>> added_dependencies;
    HashArena<P> package_store;
    DPLLPartialSolution<P, V> partial_solution;
    Id<P> next = state.root_package;
    std::unordered_map<P, Ranges<V>> not_completed_dependencies;
    package_store.alloc(package);

    std::vector<Id<P>> next_packages;

    auto all_packages = dependency_provider.packages();

    bool res = dpll_resolve<DP>(
        dependency_provider,
        partial_solution,
        package,
        package_store,
        not_completed_dependencies,
        next_packages);
    if (!res)
    {
        throw std::runtime_error("No solution found");
    }
    else
    {
        std::unordered_map<P, V> result;
        for (const auto &[pkg_id, assignment] : partial_solution.getAssignments())
        {
            const P &pkg = package_store[pkg_id];
            const VS &version_range = assignment.getAssignment().second;
            if (!version_range.segments().empty() && version_range.segments().front().first.is_finite())
            {
                result[pkg] = version_range.segments().front().first.value;
            }
        }
        return result;
    }
}