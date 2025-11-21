#include "../include/provider.h"
#include "../include/incompatibility.h"
#include "../include/core.h"

template <typename DP>
std::unordered_map<typename DP::P, typename DP::V> resolve(
    DP &dependency_provider,
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

    // Create a temporary package store to allocate the root package ID
    HashArena<P> temp_store;
    Id<P> root_pkg = temp_store.alloc(package);

    // Create the state with root package ID and version
    State<DP> state(root_pkg, version);

    // Now register the root package in state's package store
    state.package_store.alloc(package);
    std::map<Id<P>, PackageResolutionStatistics> conflict_tracker;
    std::map<Id<P>, std::set<V>> added_dependencies;
    Id<P> next = state.root_package;
    while (true)
    {
        SmallVec<std::pair<Id<P>, IncompId>> satisfier_causes = state.unit_propagation(next);

        for (const auto &pair : satisfier_causes)
        {
            Id<P> affected = pair.first;
            IncompId incompat = pair.second;
            auto &stats = conflict_tracker[affected];
            stats.unit_propagation_affected += 1;
            for (const auto &term_pair : state.incompatibility_store[incompat])
            {
                Id<P> conflict_package = term_pair.first;
                if (conflict_package == affected)
                    continue;
                conflict_tracker[conflict_package].unit_propagation_culprit += 1;
            }
        }
        auto next_pick = state.partial_solution.pick_highest_priority_pkg(
            [&](Id<P> p, const VS &range) -> typename DP::Priority
            {
                auto &stats = conflict_tracker[p];
                return dependency_provider.prioritize(
                    state.package_store[p], range, stats);
            });
        if (!next_pick)
        {
            std::unordered_map<P, V> result;
            for (const auto &kv : state.partial_solution.extract_solution())
            {
                Id<P> id = kv.first;
                const V &v = kv.second;
                // package_store[id] から実際のパッケージ名を取り出す
                result.emplace(state.package_store[id], v);
            }
            return result;
        }
        Id<P> highest_priority_pkg = next_pick->first;
        const VS *term_intersection = next_pick->second;
        next = highest_priority_pkg;

        std::optional<V> decision =
            dependency_provider.choose_version(state.package_store[next],
                                               *term_intersection);
        V v{};
        // choose_versionがversionを選ばなかった場合、term_intersecionがダメなので、no_versions
        if (!decision.has_value())
        {
            Term<V> term = Term<V>::Positive(*term_intersection);
            Incomp inc = Incompatibility<P, V, M>::no_versions(next, term);
            state.add_incompatibility(inc);
            continue;
        }
        else
            v = *decision;

        if (!term_intersection->contains(v))
        {
            std::ostringstream oss;
            throw std::logic_error(oss.str());
        }
        bool is_new_dependency = added_dependencies[next].insert(v).second;

        if (is_new_dependency)
        {
            auto deps_result =
                dependency_provider.get_dependencies(state.package_store[next], v);

            if (deps_result.tag == Availability::Unavailable)
            {
                // TODO: Handle unavailable version properly
                continue;
            }

            // Convert DependencyConstraints (map) to vector
            std::vector<std::pair<P, VS>> deps_vec;
            for (const auto &[pkg, range] : deps_result.dependencies)
            {
                deps_vec.push_back({pkg, range});
            }

            // Add dependencies - convert Id<P> to P (string)
            auto [first_incomp, end_incomp] =
                state.add_package_version_dependencies(state.package_store[next], v, deps_vec);

            // Track conflicts for all added incompatibilities
            if (first_incomp.raw < end_incomp.raw)
            {
                conflict_tracker[next].dependencies_affected += 1;
                for (uint32_t idx = first_incomp.raw; idx < end_incomp.raw; ++idx)
                {
                    IncompId incomp_id = IncompId::from(idx);
                    for (const auto &term_pair : state.incompatibility_store[incomp_id])
                    {
                        Id<P> incompat_package = term_pair.first;
                        if (incompat_package == next)
                            continue;
                        conflict_tracker[incompat_package].dependencies_culprit += 1;
                    }
                }
            }
        }
        else
        {
            state.partial_solution.add_decision(next, v);
        }
    }
}