#pragma once
#include "term.h"
#include "incompatibility.h"
#include "ranges.h"
#include "arena.h"
#include "small_map.h"
#include <set>
#include <queue>
#include <algorithm>
#include <optional>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <limits>
#define DEBUG 1

class PackageResolutionStatistics
{
public:
    size_t unit_propagation_affected = 0;
    size_t unit_propagation_culprit = 0;
    size_t dependencies_affected = 0;
    size_t dependencies_culprit = 0;
    size_t conflict_count() const
    {
        return unit_propagation_affected + unit_propagation_culprit + dependencies_affected + dependencies_culprit;
    }
};

class DecisionLevel
{
public:
    size_t level{0};

    DecisionLevel() = default;
    explicit DecisionLevel(size_t l) : level(l) {}

    DecisionLevel increment() const { return DecisionLevel(level + 1); }

    bool operator>(const DecisionLevel &other) const { return level > other.level; }
    bool operator>=(const DecisionLevel &other) const { return level >= other.level; }
    bool operator<(const DecisionLevel &other) const { return level < other.level; }
    bool operator<=(const DecisionLevel &other) const { return level <= other.level; }
    bool operator==(const DecisionLevel &other) const { return level == other.level; }
};

enum class Availability
{
    Available,
    Unavailable,
};

template <class P, class V, class M>
struct Dependencies
{
    using VS = Ranges<V>;
    using DependencyConstraints = std::map<P, VS>;
    Availability tag;
    DependencyConstraints dependencies;
    M meta;

    static Dependencies<P, V, M> available(const DependencyConstraints &deps, const M &meta)
    {
        return Dependencies<P, V, M>{Availability::Available, deps, meta};
    }
    static Dependencies<P, V, M> unavailable(const M &meta)
    {
        return Dependencies<P, V, M>{Availability::Unavailable, {}, meta};
    }
};

template <class P, class V, class M, class Priority>
class DependencyProvider
{
public:
    using VS = Ranges<V>;
    using TermType = Term<V>;
    using DependencyConstraints = std::map<P, VS>;

    virtual Priority prioritize(const P &package, const VS &range, const PackageResolutionStatistics &package_conflicts_counts) = 0;
    virtual std::optional<V> choose_version(const P &package, const VS &range) = 0;
    virtual Dependencies<P, V, M> get_dependencies(const P &package, const V &version) = 0;
};

template <class P_, class V_>
class OfflineDependencyProvider : public DependencyProvider<P_, V_, std::string, std::pair<std::uint32_t, std::int32_t>>
{
public:
    using P = P_;
    using V = V_;
    using M = std::string;
    using VS = Ranges<V>;
    using TermType = Term<V>;
    using DependencyConstraints = std::map<P, VS>;
    using VersionMap = std::map<V, DependencyConstraints>;
    using PackageDeps = std::unordered_map<P, VersionMap>;
    using Priority = std::pair<std::uint32_t, std::int32_t>;

public:
    // ここにはとりあえずユーザが設定した全てのdependency情報を保存しておく
    PackageDeps dependencies_;

public:
    OfflineDependencyProvider() : dependencies_() {}

    template <class InputIt>
    void add_dependencies(const P &package, const V &version, InputIt begin, InputIt end)
    {
        DependencyConstraints deps;
        for (auto it = begin; it != end; ++it)
        {
            deps.emplace(it->first, it->second);
        }
        dependencies_[package][version] = std::move(deps);
    }

    void add_dependencies(const P &package, const V &version, std::initializer_list<std::pair<P, VS>> deps)
    {
        add_dependencies(package, version, deps.begin(), deps.end());
    }
    std::vector<P> packages() const
    {
        std::vector<P> result;
        result.reserve(dependencies_.size());
        for (const auto &kv : dependencies_)
        {
            result.push_back(kv.first);
        }
        return result;
    }
    std::optional<std::vector<V>> versions(const P &package) const
    {
        auto it = dependencies_.find(package);
        if (it == dependencies_.end())
            return std::nullopt;
        std::vector<V> vers;
        for (const auto &kv : it->second)
        {
            vers.push_back(kv.first);
        }
        return vers;
    }
    std::optional<DependencyConstraints> dependencies(const P &package, const V &version) const
    {
        auto it = dependencies_.find(package);
        if (it == dependencies_.end())
            return std::nullopt;
        auto ver_it = it->second.find(version);
        if (ver_it == it->second.end())
            return std::nullopt;
        return ver_it->second;
    }
    std::optional<V> choose_version(const P &package, const VS &range) override
    {
        auto it = dependencies_.find(package);
        if (it == dependencies_.end())
            return std::nullopt;
        const VersionMap &ver_map = it->second;
        // dependencyの左辺に登録されているversionのうち、rangeに含まれる最大のversionを返す
        for (auto it2 = ver_map.rbegin(); it2 != ver_map.rend(); ++it2)
        {
            const V &ver = it2->first;
            if (range.contains(ver))
            {
                return ver;
            }
        }
        return std::nullopt;
    }
    Dependencies<P, V, M> get_dependencies(const P &package, const V &version) override
    {
        auto it = dependencies_.find(package);
        if (it == dependencies_.end())
        {
            return Dependencies<P, V, M>::unavailable("Package not found");
        }
        auto ver_it = it->second.find(version);
        if (ver_it == it->second.end())
        {
            return Dependencies<P, V, M>::unavailable("Version not found");
        }
        return Dependencies<P, V, M>::available(ver_it->second, "OK");
    }
    // packageのdependencyを見たときに、packageの今決まっているversion範囲に基づいて優先度を計算する
    // コンフリクトが多いpackageや許容されるversionが少ないpackageほど優先度が高くなるようにする
    Priority prioritize(const P &package, const VS &range, const PackageResolutionStatistics &package_conflicts_counts) override
    {
        std::int32_t version_count = 0;
        auto it_pkg = dependencies_.find(package);
        if (it_pkg != dependencies_.end())
        {
            const VersionMap &ver_map = it_pkg->second;
            for (const auto &kv : ver_map)
            {
                const V &v = kv.first;
                if (range.contains(v))
                    version_count += 1;
            }
        }
        if (version_count == 0)
            return {std::numeric_limits<std::uint32_t>::max(), 0};
        return {static_cast<std::uint32_t>(package_conflicts_counts.conflict_count()), -version_count};
    }
};

template <class P, class V, class M>
struct DatedDerivation
{
    using VS = Ranges<V>;
    using TermT = Term<V>;
    using IncompId = Id<Incompatibility<P, V, M>>;

    uint32_t global_index;          // next_global_index
    DecisionLevel decision_level;   // その時点の decision level
    IncompId cause;                 // どの incompatibility による導出か
    TermT accumulated_intersection; // その時点までの term の積
};

template <class V>
struct AssignmentsIntersection
{
    using VS = Ranges<V>;
    enum class Kind
    {
        Decision,
        Derivations
    } kind;

    uint32_t decision_global_index = 0;
    V version{};
    Term<V> term;

    static AssignmentsIntersection makeDecision(uint32_t idx, const V &v)
    {
        AssignmentsIntersection a;
        a.kind = Kind::Decision;
        a.decision_global_index = idx;
        a.version = v;
        a.term = Term<V>::exact(v);
        return a;
    }

    static AssignmentsIntersection makeDerivations(const Term<V> &t)
    {
        AssignmentsIntersection a;
        a.kind = Kind::Derivations;
        a.term = t;
        return a;
    }

    bool isDecision() const { return kind == Kind::Decision; }
    bool isDerivations() const { return kind == Kind::Derivations; }

    const Term<V> &term_ref() const { return term; }

    const VS *potential_package_filter() const
    {
        if (isDecision())
            return nullptr;

        if (term.is_positive())
            return term.unwrap_positive_ptr();
        return nullptr;
    }
};

// 単純にあるpackageに対するversionの集合なんだが、それが導出された過程が記録されているもの
template <class P, class V, class M>
struct PackageAssignments
{
    using VS = Ranges<V>;
    using TermT = Term<V>;
    using IncompId = Id<Incompatibility<P, V, M>>;

    // 今までの導出されたversion集合の積集合
    AssignmentsIntersection<V> assignments_intersection;

    std::vector<DatedDerivation<P, V, M>> dated_derivations;

    DecisionLevel smallest_decision_level{0};
    DecisionLevel highest_decision_level{0};

    // 初めてstart_termとdisjointになる瞬間を返す
    std::tuple<std::optional<IncompId>, uint32_t, DecisionLevel>
    satisfier(Id<P> package, const TermT &start_term) const
    {
        TermT empty = TermT::empty();

        auto it = std::find_if(
            dated_derivations.begin(), dated_derivations.end(),
            [&](const DatedDerivation<P, V, M> &dd)
            {
                return dd.accumulated_intersection.is_disjoint(start_term);
            });

        if (it != dated_derivations.end())
        {
            auto intersect = it->accumulated_intersection.intersection(start_term);
            (void)empty;

            return {std::make_optional(it->cause), it->global_index, it->decision_level};
        }

        if (assignments_intersection.isDecision())
        {
            return {
                std::nullopt,
                assignments_intersection.decision_global_index,
                highest_decision_level};
        }

        // Derivationsの場合、disjointになる時点が見つからなかった
        // これは最初からincompat_termを満たしていたことを意味する
        // 最初の導出時点を返す
        if (!dated_derivations.empty())
        {
            const auto &first_dd = dated_derivations.front();
            return {std::make_optional(first_dd.cause), first_dd.global_index, first_dd.decision_level};
        }

        const auto &accum = assignments_intersection.term;
        throw std::logic_error(
            "PackageAssignments::satisfier: expected decision, got derivations with empty dated_derivations "
            "(Version ordering or invariants may be broken)");
    }
    friend std::ostream &operator<<(std::ostream &os, const PackageAssignments &pa)
    {
        os << "PackageAssignments(";
        // os << "assignments_intersection=";
        if (pa.assignments_intersection.isDecision())
        {
            os << "Decision(version=" << pa.assignments_intersection.version << ")";
        }
        else
        {
            os << "Derivations(term=" << pa.assignments_intersection.term << ")";
        }
        // os << ", dated_derivations=[";
        // bool first = true;
        // for (const auto &dd : pa.dated_derivations)
        // {
        //     if (!first)
        //         os << ", ";
        //     first = false;
        //     os << "{global_index=" << dd.global_index
        //        << ", decision_level=" << dd.decision_level.level
        //        << ", cause=" << dd.cause
        //        << ", accumulated_intersection=" << dd.accumulated_intersection
        //        << "}";
        // }
        // os << "]";
        // os << ", smallest_decision_level=" << pa.smallest_decision_level.level;
        // os << ", highest_decision_level=" << pa.highest_decision_level.level;
        os << ")";
        return os;
    }
};

template <class P, class V, class M>
struct SatisfierSearch
{
    using VS = Ranges<V>;
    using IncompId = Id<Incompatibility<P, V, M>>;

    enum class Kind
    {
        DifferentDecisionLevels,
        SameDecisionLevels
    } kind;

    DecisionLevel previous_satisfier_level{}; // for DifferentDecisionLevels
    IncompId satisfier_cause{};               // for SameDecisionLevels

    static SatisfierSearch Different(DecisionLevel lvl)
    {
        SatisfierSearch s;
        s.kind = Kind::DifferentDecisionLevels;
        s.previous_satisfier_level = lvl;
        return s;
    }

    static SatisfierSearch Same(const IncompId &cause)
    {
        SatisfierSearch s;
        s.kind = Kind::SameDecisionLevels;
        s.satisfier_cause = cause;
        return s;
    }
};

template <class P, class V, class M, class Priority>
class PartialSolution : public DependencyProvider<P, V, M, Priority>
{
public:
    using VS = Ranges<V>;
    using Incomp = Incompatibility<P, V, M>;
    using IncompId = Id<Incompatibility<P, V, M>>;
    using Information = std::tuple<std::optional<IncompId>, uint32_t, DecisionLevel>;
    using SatisfiedMap = SmallMap<Id<P>, Information>;

    uint32_t next_global_index{0};
    DecisionLevel current_decision_level{0};
    bool has_ever_backtracked{false};
    // 一つのpackageに対しては高々1つのPackageAssignmentsを持つ
    // 解集合
    std::vector<std::pair<Id<P>, PackageAssignments<P, V, M>>> package_assignments;
    std::unordered_map<Id<P>, size_t> package_assignments_index_map;
    std::priority_queue<
        std::tuple<Priority, uint32_t, Id<P>>,
        std::vector<std::tuple<Priority, uint32_t, Id<P>>>,
        std::less<>>
        prioritized_potential_packages;

    // まだ優先度が更新されていないpackage
    std::set<Id<P>> outdated_priorities;
    PartialSolution() = default;
    PartialSolution empty() { return PartialSolution(); }

    std::optional<V> choose_version(const P &package, const VS &range) override
    {
        (void)package;
        (void)range;
        return std::nullopt;
    }

    Priority prioritize(const P &package, const VS &range, const PackageResolutionStatistics &stats) override
    {
        (void)package;
        (void)range;
        return {static_cast<std::uint32_t>(stats.conflict_count()), 0};
    }

    Dependencies<P, V, M> get_dependencies(const P &package, const V &version) override
    {
        (void)package;
        (void)version;
        return Dependencies<P, V, M>::unavailable("Not implemented");
    }

    PackageAssignments<P, V, M> *find_package_assignments(Id<P> package)
    {
        if (auto it = package_assignments_index_map.find(package); it != package_assignments_index_map.end())
            return &package_assignments[it->second].second;
        return nullptr;
    }
    const PackageAssignments<P, V, M> *find_package_assignments(Id<P> package) const
    {
        if (auto it = package_assignments_index_map.find(package); it != package_assignments_index_map.end())
            return &package_assignments[it->second].second;
        return nullptr;
    }

    size_t index_of(Id<P> package) const
    {
        if (auto it = package_assignments_index_map.find(package); it != package_assignments_index_map.end())
            return it->second;
        return static_cast<size_t>(-1);
    }
    template <typename IncompRange>
    std::optional<IncompId>
    add_package_version_incompatibilities(
        Id<P> package,
        const V &version,
        const IncompRange &new_incompatibilities,
        const Arena<Incomp> &store,
        const HashArena<P> &package_store)
    {
        // まだ一度も backtrack していないなら fast path:
        // 依存関係をチェックせずにそのまま決定として追加してしまう。
        if (!has_ever_backtracked)
        {
            add_decision(package, version);
            return std::nullopt;
        }

        // 依存 incompat がこの (package, version) を禁止していないか調べる。
        Term<V> package_term = Term<V>::exact(version);

        auto relation = [&](IncompId incompat_id) -> IncompatRelation<P>
        {
            const Incomp &inc = store[incompat_id];
            return inc.relation([&](Id<P> p) -> const Term<V> *
                                {
                // まだ partial_solution にはこの package の決定は入っていないので、
                // 自分自身については「これから決めたい version を表す term」を返す。
                if (p == package) {
                    return &package_term;
                } else {
                    // 他のパッケージについては、現在の partial_solution の
                    // term_intersection を参照する（なければ nullptr）。
                    return term_intersection_for_package(p);
                } });
        };

        // 追加された incompat 群の中に、
        // 「今の partial_solution + (package=version) を完全に満たしてしまう
        // ＝ conflict になる」ものがあるか探す。
        for (IncompId incompat_id : new_incompatibilities)
        {
            if (relation(incompat_id).tag == IncompatRelationTag::Satisfied)
            {
                // このバージョンを決定すると依存関係と矛盾するので reject
                return incompat_id; // 衝突した incompat を返す
            }
        }

        // どの incompat も conflict を起こさなかったので、この package@version を決定として追加
        add_decision(package, version);
        return std::nullopt;
    }

    void add_decision(Id<P> package, const V &version)
    {
#ifdef DEBUG
        {
            auto *pa = find_package_assignments(package);
            if (!pa)
                throw std::logic_error("add_decision: package assignments not found");
            if (pa->assignments_intersection.isDecision())
                throw std::logic_error("add_decision: package already has a decision");

            const auto &term = pa->assignments_intersection.term;
            if (!term.contains(version))
            {
                std::ostringstream oss;
                oss << "add_version version not contained in term";
                throw std::logic_error(oss.str());
            }
        }
#endif
        size_t new_idx = current_decision_level.level;
        current_decision_level = current_decision_level.increment();
        auto *pa = find_package_assignments(package);
        if (!pa)
            throw std::logic_error("add_decision: package assignments not found");
        size_t old_idx = index_of(package);
        pa->highest_decision_level = current_decision_level;
        pa->assignments_intersection = AssignmentsIntersection<V>::makeDecision(next_global_index, version);
        if (old_idx != new_idx)
        {
            package_assignments_index_map[package] = new_idx;
            package_assignments_index_map[package_assignments[new_idx].first] = old_idx;
            std::swap(package_assignments[old_idx], package_assignments[new_idx]);
        }
        next_global_index += 1;
    }

    // causeによって導出されたことを記録する
    void add_derivation(Id<P> package, IncompId cause, const Arena<Incomp> &store, HashArena<P> package_store)
    {
        DatedDerivation<P, V, M> dd{
            next_global_index,
            current_decision_level,
            cause,
            store[cause].get(package)->negate()};
        next_global_index += 1;

        auto *pa = find_package_assignments(package);
        if (pa)
        {
            pa->highest_decision_level = current_decision_level;
            if (pa->assignments_intersection.isDecision())
            {
                throw std::logic_error("add_derivation: package already has a decision");
            }
            Term<V> new_accumulated = pa->assignments_intersection.term;
            new_accumulated = new_accumulated.intersection(dd.accumulated_intersection);
            dd.accumulated_intersection = new_accumulated;
            if (new_accumulated.is_positive())
                outdated_priorities.insert(package);
            pa->dated_derivations.push_back(std::move(dd));
            pa->assignments_intersection = AssignmentsIntersection<V>::makeDerivations(new_accumulated);
        }
        else
        {
            Term<V> term = dd.accumulated_intersection;
            if (term.is_positive())
                outdated_priorities.insert(package);

            PackageAssignments<P, V, M> pa;
            pa.smallest_decision_level = current_decision_level;
            pa.highest_decision_level = current_decision_level;
            pa.dated_derivations.push_back(std::move(dd));
            pa.assignments_intersection = AssignmentsIntersection<V>::makeDerivations(term);
            package_assignments.emplace_back(package, std::move(pa));
            package_assignments_index_map[package] = package_assignments.size() - 1;
        }
    }
    template <class Prioritizer>
    std::optional<std::pair<Id<P>, const VS *>>
    pick_highest_priority_pkg(Prioritizer prioritizer)
    {
        for (auto it = outdated_priorities.begin(); it != outdated_priorities.end();)
        {
            Id<P> p = *it;
            it = outdated_priorities.erase(it);
            auto pa_it = find_package_assignments(p);
            if (!pa_it)
                continue;
            const VS *r = pa_it->assignments_intersection.potential_package_filter();
            if (!r)
                continue;
            // packageとそのversion範囲に基づいて優先度を計算してキューに追加
            Priority prio = prioritizer(p, *r);
            prioritized_potential_packages.emplace(prio, static_cast<uint32_t>(p.into_raw()), p);
        }
        while (!prioritized_potential_packages.empty())
        {
            auto [prio, order, p] = prioritized_potential_packages.top();
            (void)prio;
            (void)order;
            prioritized_potential_packages.pop();
            auto pa_it = find_package_assignments(p);
            if (!pa_it)
                continue;
            const VS *r = pa_it->assignments_intersection.potential_package_filter();
            if (r)
            {
                return std::make_optional(std::make_pair(p, r));
            }
        }
        return std::nullopt;
    }
    std::vector<std::pair<Id<P>, V>> extract_solution()
    {
        std::vector<std::pair<Id<P>, V>> solution;
        for (const auto &kv : package_assignments)
        {
            const Id<P> &package = kv.first;
            const auto &pa = kv.second;
            if (pa.assignments_intersection.isDecision())
            {
                solution.emplace_back(package, pa.assignments_intersection.version);
            }
            else
            {
                std::ostringstream oss;
                oss << "Derivations in the Decision part. Decision level "
                    << current_decision_level.level << "\n";
                for (std::size_t j = 0; j < package_assignments.size(); ++j)
                {
                    const auto &e = package_assignments[j];
                    oss << " * Package ID: " << e.first.into_raw() << " " << e.second.assignments_intersection.term_ref() << "\n";
                }
                throw std::logic_error(oss.str());
            }
        }
        return solution;
    }
    // packageに対する導出の履歴を辿ってdecision_levelよりも完全に後のものは削除、完全に前のものは保存、
    // そうでないものは後ろから見ていって、decision_levelより小さくなるまで導出を元に戻していく
    void backtrack(DecisionLevel decision_level)
    {
        current_decision_level = decision_level;

        std::vector<std::pair<Id<P>, PackageAssignments<P, V, M>>> new_vec;
        new_vec.reserve(package_assignments.size());
        for (auto &entry : package_assignments)
        {
            Id<P> p = entry.first;
            auto &pa = entry.second;
            if (pa.smallest_decision_level > decision_level)
            {
                continue;
            }
            else if (pa.highest_decision_level <= decision_level)
            {
                if (pa.assignments_intersection.potential_package_filter())
                {
                    outdated_priorities.insert(p);
                }
                new_vec.push_back(std::move(entry));
            }
            else
            {
                while (!pa.dated_derivations.empty() &&
                       pa.dated_derivations.back().decision_level > decision_level)
                {
                    pa.dated_derivations.pop_back();
                }
                if (pa.dated_derivations.empty())
                    continue;
                const auto &last = pa.dated_derivations.back();
                pa.highest_decision_level = last.decision_level;
                pa.assignments_intersection = AssignmentsIntersection<V>::makeDerivations(last.accumulated_intersection);
                if (pa.assignments_intersection.term_ref().is_positive())
                {
                    outdated_priorities.insert(p);
                }
                new_vec.push_back(std::move(entry));
            }
        }
        package_assignments.swap(new_vec);
        package_assignments_index_map.clear();
        for (size_t i = 0; i < package_assignments.size(); ++i)
        {
            package_assignments_index_map[package_assignments[i].first] = i;
        }
        has_ever_backtracked = true;
    }
    // あるpackageに対してそれに対するバージョンの集合を返す(Assingments_intersectionは高々1つ)
    const Term<V> *term_intersection_for_package(Id<P> package) const
    {
        for (const auto &kv : package_assignments)
        {
            if (kv.first == package)
            {
                return &kv.second.assignments_intersection.term_ref();
            }
        }
        return nullptr;
    }

    // 引数のincompatibilityと現在導出されているpackageとの間の関係
    // Incompatibilityの全ての制約が満たされているならSatisfied (わかりにくいが、これがコンフリクトが起きている状態、Incompatibilityは全て満たされるいる状態ではだめ)
    // 一つだけ満たされていないならAlmostSatisfied
    // 二つ以上満たされていないならContradicted
    // そもそもまだ決まっていないpackageがあって判断できないならInconclusive
    IncompatRelation<P> relation(const Incomp &incompat) const
    {
        return incompat.relation([&](Id<P> pkg)
                                 { return term_intersection_for_package(pkg); });
    }
    // Incompatibilityの全ての制約の中で一つでも制約を満たさなければ発火しないので、
    // 全てのパッケージに対してそのような制約をPartial Solutionが満たす状況を探している
    template <class PackageStore>
    SatisfiedMap find_satisfier(const Incomp &incompat, const PackageStore &pkgs) const
    {
        SatisfiedMap satisfied;
        // std::cout << "find_satisfier called for: " << incompat.display(pkgs) << "\n";
        for (auto it = incompat.begin(); it != incompat.end(); ++it)
        {
            Id<P> package = it->first;
            const Term<V> &incompat_term = it->second;

            const auto *pa = find_package_assignments(package);
            if (!pa)
                throw std::logic_error("find_satisfier: package assignments not found");
            Information info = pa->satisfier(package, incompat_term.negate());
            satisfied.insert(package, info);
        }
        return satisfied;
    }
    // Incompatibilityを満たすpackageの中で、最も最近のdecision levelで満たしているpackageを探す
    template <class PackageStore>
    std::pair<Id<P>, SatisfierSearch<P, V, M>>
    satisfier_search(const Incomp &incompat, const Arena<Incomp> &store, const PackageStore &pkgs) const
    {
        auto satisfied_map = find_satisfier(incompat, pkgs);
        Id<P> satisfier_package{};
        Information satisfier_info{};
        uint32_t max_gidx = 0;

        // 一番満たす制約でglobal indexが大きいものを探す
        for (auto it = satisfied_map.begin(); it != satisfied_map.end(); ++it)
        {
            auto [cause, gidx, dl] = it->second;
            if (gidx >= max_gidx)
            {
                max_gidx = gidx;
                satisfier_package = it->first;
                satisfier_info = it->second;
            }
        }
        auto [satisfier_cause, _, satisfier_decision_level] = satisfier_info;
        (void)_;
        DecisionLevel prev_level = find_previous_satisfier(incompat, satisfier_package, satisfied_map, store, pkgs);
        if (prev_level >= satisfier_decision_level)
        {
            return {satisfier_package,
                    SatisfierSearch<P, V, M>::Same(*satisfier_cause)};
        }
        else
        {
            return {satisfier_package,
                    SatisfierSearch<P, V, M>::Different(prev_level)};
        }
    }

    // 与えられたIncompatibilityのpackageのversionと原因のIncompatibilityのpackageのversoin
    // をどちらもnegateして満たさない状況を、作った時に、
    // その条件が直近どのDecision levelで満たされているかを測る
    DecisionLevel find_previous_satisfier(
        const Incomp &incompat,
        Id<P> satisfier_package,
        SatisfiedMap &satisfied_map,
        const Arena<Incomp> &store,
        const HashArena<P> &package_store) const
    {
        const auto *satisfier_pa = find_package_assignments(satisfier_package);
        if (!satisfier_pa)
            throw std::logic_error("find_previous_satisfier: satisfier package not found");

        const Information *info_ptr = satisfied_map.get(satisfier_package);
        if (!info_ptr)
            throw std::logic_error("find_previous_satisfier: satisfier not in map");

        auto &[satisfier_cause_opt, _gidx, _dl] = *info_ptr;
        (void)_gidx;
        (void)_dl;

        Term<V> accum_term;
        // 原因となっているpackageのversion集合を取得する
        // その原因となっているversionをnegateしたものとincompatibilityのversion集合の積集合をとる
        if (satisfier_cause_opt.has_value())
        {
            const auto &cause = *satisfier_cause_opt;
            accum_term = store[cause].get(satisfier_package)->negate();
        }
        else
        {
            // 原因がdecisionの場合はsatisfier_cause_optはnulloptになる
            if (!satisfier_pa->assignments_intersection.isDecision())
                throw std::logic_error("previous_satisfier: must be a decision if no cause");
            accum_term = satisfier_pa->assignments_intersection.term;
        }

        const Term<V> &incompat_term = *incompat.get(satisfier_package);

        Term<V> new_term = accum_term.intersection(incompat_term.negate());
        auto info = satisfier_pa->satisfier(satisfier_package, new_term);
        satisfied_map.insert(satisfier_package, info);

        DecisionLevel max_dl{0};
        for (auto it2 = satisfied_map.begin(); it2 != satisfied_map.end(); ++it2)
        {
            DecisionLevel dl = std::get<2>(it2->second);
            if (dl > max_dl)
                max_dl = dl;
        }

        if (max_dl < DecisionLevel(1))
            return DecisionLevel(1);
        return max_dl;
    }
    DecisionLevel current_decision_level_value() const { return current_decision_level; }
};