#pragma once
#include "term.h"
#include "incompatibility.h"
#include "ranges.h"
#include "arena.h"
#include "small_map.h"
#include "provider.h"
#include "types.h"

template <class DP>
class State
{
    using Types = PubGrubTypes<DP>;
    using P = typename Types::P;
    using V = typename Types::V;
    using M = typename Types::M;
    using VS = typename Types::VS;
    using Incomp = typename Types::Incomp;
    using IncompId = typename Types::IncompId;

public:
    Id<P> root_package;
    V root_version;

    std::unordered_map<Id<P>, std::vector<IncompId>> incompatibilities;
    // もうfalseが確定したIncompatibilityの記録
    std::unordered_map<IncompId, DecisionLevel> contradicted_incompatibilities;
    struct PairHash
    {
        std::size_t operator()(const std::pair<Id<P>, Id<P>> &pr) const noexcept
        {
            return std::hash<Id<P>>{}(pr.first) ^ (std::hash<Id<P>>{}(pr.second) << 1);
        }
    };
    // 同じ依存元と依存先のpackageのIDをもったIncompatibilityをまとめておくためのもの
    std::unordered_map<std::pair<Id<P>, Id<P>>, std::vector<IncompId>, PairHash> merged_dependencies;
    PartialSolution<P, V, M, typename DP::Priority> partial_solution;
    // 全てのIncompatibilityのstore
    Arena<Incomp> incompatibility_store;
    HashArena<P> package_store;
    std::vector<Id<P>> unit_propagation_buffer;

    State() : root_package(), root_version()
    {
        incompatibilities = std::unordered_map<Id<P>, std::vector<IncompId>>();
        contradicted_incompatibilities = std::unordered_map<IncompId, DecisionLevel>();
        merged_dependencies = std::unordered_map<std::pair<Id<P>, Id<P>>, std::vector<IncompId>, PairHash>();
        partial_solution = PartialSolution<P, V, M, typename DP::Priority>();
        incompatibility_store = Arena<Incomp>();
        package_store = HashArena<P>();
        unit_propagation_buffer = std::vector<Id<P>>();
    }

    static State init(const P &root_pkg, const V &root_ver)
    {
        State state;
        state.root_package = state.package_store.alloc(root_pkg);
        state.root_version = root_ver;

        // Create not_root incompatibility
        Incomp not_root = Incomp::not_root(state.root_package, root_ver);
        IncompId not_root_id = state.incompatibility_store.alloc(std::move(not_root));

        // Add to incompatibilities map
        state.incompatibilities[state.root_package] = {not_root_id};

        return state;
    }

    std::optional<IncompId> add_package_version_dependencies(const P &package, const V &version, const std::vector<std::pair<P, VS>> &deps)
    {
        std::uint32_t start_raw = incompatibility_store.size();
        for (const auto &entry : deps)
        {
            const P &dep_p = entry.first;
            const VS &dep_v = entry.second;

            Id<P> dep_pid = package_store.alloc(dep_p);
            Incomp incompat = Incomp::from_dependency(
                package_store.alloc(package),
                Ranges<V>::singleton(version),
                std::make_pair(dep_pid, dep_v));
            IncompId id = incompatibility_store.alloc(std::move(incompat));
            merge_incompatibility(id);
        }
        std::uint32_t end_raw = incompatibility_store.size();
        return partial_solution.add_package_version_incompatibilities(
            package_store.alloc(package),
            version,
            IdRange<Incomp>{IncompId::from(start_raw), IncompId::from(end_raw)},
            incompatibility_store,
            package_store);
    }
    // あるincompatibilityを他のincompatibilityとmerge
    // a@1とa@2がbに依存しているときに、a@1 || a@2 がbに依存しているものとしてマージする
    void merge_incompatibility(IncompId incomp)
    {
        const Incomp &incompat = incompatibility_store[incomp];
        auto dep = incompat.as_dependency();
        if (dep)
        {
            auto [p1, p2] = *dep;
            auto &deps_list = merged_dependencies[std::make_pair(p1, p2)];
            bool merged_any = false;
            for (auto &past_id : deps_list)
            {
                auto maybe_merged = incompatibility_store[incomp].merge_dependents(incompatibility_store[past_id]);
                if (maybe_merged)
                {
                    IncompId new_id = incompatibility_store.alloc(std::move(*maybe_merged));
                    // 既存のincompatibilityの削除
                    for (const auto &[pkg, _term] : incompatibility_store[new_id])
                    {
                        (void)_term;
                        auto it_list = incompatibilities.find(pkg);
                        if (it_list == incompatibilities.end())
                            continue;
                        auto &list = it_list->second;
                        list.erase(std::remove(list.begin(), list.end(), past_id), list.end());
                    }
                    past_id = new_id;
                    incomp = new_id;
                    merged_any = true;
                }
            }
            if (!merged_any)
                deps_list.push_back(incomp);
        }
        // incompの中の(package, version)の組に対して、incompatibilities[package]にincompを保管する
        for (const auto &[pkg, term] : incompatibility_store[incomp])
        {
            (void)term;
            incompatibilities[pkg].push_back(incomp);
        }
    }

    void add_incompatibility(const Incomp &incompat)
    {
        IncompId id = incompatibility_store.alloc(incompat);
        merge_incompatibility(id);
    }

    void backtrack(IncompId incompat, bool incompat_changed, DecisionLevel decision_level)
    {
        partial_solution.backtrack(decision_level);
        for (auto it = contradicted_incompatibilities.begin(); it != contradicted_incompatibilities.end();)
        {
            if (it->second > decision_level)
            {
                it = contradicted_incompatibilities.erase(it);
            }
            else
            {
                ++it;
            }
        }
        if (incompat_changed)
        {
            merge_incompatibility(incompat);
        }
    }

    SmallVec<std::pair<Id<P>, IncompId>> unit_propagation(Id<P> &package)
    {
        unit_propagation_buffer.clear();
        unit_propagation_buffer.push_back(package);
        SmallVec<std::pair<Id<P>, IncompId>> satisfier_causes;
        while (!unit_propagation_buffer.empty())
        {
            Id<P> current_package = unit_propagation_buffer.back();
            unit_propagation_buffer.pop_back();
            std::optional<IncompId> conflict_id = std::nullopt;

            // Get incompatibilities for this package
            auto it = incompatibilities.find(current_package);
            if (it == incompatibilities.end())
                continue;

            const auto &pkg_incompats = it->second;
            for (auto rit = pkg_incompats.rbegin(); rit != pkg_incompats.rend(); ++rit)
            {
                IncompId incompat_id = *rit;
                Incomp &incompat = incompatibility_store[incompat_id];
                auto rel = partial_solution.relation(incompat);
                if (contradicted_incompatibilities.find(incompat_id) != contradicted_incompatibilities.end())
                    continue;

                // incompatibilityをpartial_solutionが満たしているならコンフリクト
                if (rel.tag == IncompatRelationTag::Satisfied)
                {
                    conflict_id = incompat_id;
                }
                // 一つだけ満たされていない状態があるのであれば、Incompatibilityを満たさないようにするためには、
                // そのpackageの状態のnegateしたものをderivationとして追加する必要がある
                else if (rel.tag == IncompatRelationTag::AlmostSatisfied)
                {
                    auto package_almost = rel.pkg.value();
                    // Check if already in buffer using std::find
                    if (std::find(unit_propagation_buffer.begin(), unit_propagation_buffer.end(), package_almost) == unit_propagation_buffer.end())
                    {
                        unit_propagation_buffer.push_back(package_almost);
                    }
                    partial_solution.add_derivation(package_almost, incompat_id, incompatibility_store, package_store);

                    contradicted_incompatibilities[incompat_id] = partial_solution.current_decision_level;
                }
                else if (rel.tag == IncompatRelationTag::Contradicted)
                {
                    contradicted_incompatibilities[incompat_id] = partial_solution.current_decision_level;
                }
            }
            if (conflict_id)
            {
                auto result = conflict_resolution(*conflict_id, satisfier_causes);
                if (!result)
                    throw std::runtime_error("Conflict at root package during unit propagation");
                Id<P> package_almost = result->first;
                IncompId root_cause = result->second;
                unit_propagation_buffer.clear();
                unit_propagation_buffer.push_back(package_almost);
                partial_solution.add_derivation(package_almost, root_cause, incompatibility_store, package_store);
                contradicted_incompatibilities[root_cause] = partial_solution.current_decision_level;
            }
        }
        return satisfier_causes;
    }

    std::optional<std::pair<Id<P>, IncompId>> conflict_resolution(
        IncompId incompatibility, SmallVec<std::pair<Id<P>, IncompId>> &satisfier_causes)
    {
        IncompId current_incompat_id = incompatibility;
        bool current_incompat_changed = false;
        while (true)
        {
            if (incompatibility_store[current_incompat_id].is_terminal(root_package, root_version))
            {
                return std::nullopt;
            }
            else
            {
                incompatibility_store[current_incompat_id].display(package_store);
                auto [package, satisfier_search_result] = partial_solution.satisfier_search(
                    incompatibility_store[current_incompat_id], incompatibility_store, package_store);
                if (satisfier_search_result.kind == SatisfierSearch<P, V, M>::Kind::DifferentDecisionLevels)
                {
                    backtrack(current_incompat_id, current_incompat_changed, satisfier_search_result.previous_satisfier_level);
                    satisfier_causes.push(std::make_pair(package, current_incompat_id));
                    return std::make_optional(std::make_pair(package, current_incompat_id));
                }
                else if (satisfier_search_result.kind == SatisfierSearch<P, V, M>::Kind::SameDecisionLevels)
                {
                    Incomp prior_cause = Incompatibility<P, V, M>::prior_cause(
                        current_incompat_id, satisfier_search_result.satisfier_cause, package, incompatibility_store);
                    current_incompat_id = incompatibility_store.alloc(std::move(prior_cause));
                    satisfier_causes.push(std::make_pair(package, current_incompat_id));
                    current_incompat_changed = true;
                }
            }
        }
    }
};