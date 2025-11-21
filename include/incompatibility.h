#pragma once
#include "term.h"
#include "versions.h"
#include "ranges.h"
#include "arena.h"
#include "small_map.h"
#include <sstream>
#include <string>

enum class IncompatRelationTag
{
    Satisfied,
    Contradicted,
    AlmostSatisfied,
    Inconclusive
};

template <class P>
struct IncompatRelation
{
    IncompatRelationTag tag;
    std::optional<Id<P>> pkg;
    static IncompatRelation Satisfied() { return IncompatRelation{IncompatRelationTag::Satisfied, std::nullopt}; }
    static IncompatRelation Contradicted(Id<P> p) { return IncompatRelation{IncompatRelationTag::Contradicted, p}; }
    static IncompatRelation AlmostSatisfied(Id<P> p) { return IncompatRelation{IncompatRelationTag::AlmostSatisfied, p}; }
    static IncompatRelation Inconclusive() { return IncompatRelation{IncompatRelationTag::Inconclusive, std::nullopt}; }
};

template <class P, class V, class M>
class Incompatibility
{
    using VS = Ranges<V>;
    using PId = Id<P>;
    using Self = Incompatibility<P, V, M>;
    using IncompId = Id<Self>;

public:
    struct NotRoot
    {
        Id<P> pkg;
        V version;
    };
    struct NoVersions
    {
        Id<P> pkg;
        VS ranges;
    };
    struct FromDependencyOf
    {
        Id<P> pkg1;
        VS ranges1;
        Id<P> pkg2;
        VS ranges2;
    };
    struct DerivedFrom
    {
        IncompId base1;
        IncompId base2;
    };
    struct Custom
    {
        Id<P> pkg;
        VS ranges;
        M metadata;
    };
    using Kind = std::variant<NotRoot, NoVersions, FromDependencyOf, DerivedFrom, Custom>;

private:
    SmallMap<PId, Term<V>> terms_;
    Kind kind_;

public:
    Incompatibility() = default;
    Incompatibility(SmallMap<Id<P>, Term<V>> terms, Kind kind)
        : terms_(std::move(terms)), kind_(std::move(kind))
    {
    }
    static Self not_root(PId pkg, const V &version)
    {
        SmallMap<PId, Term<V>> mp;
        mp.insert(pkg, Term<V>::Negative(Ranges<V>::singleton(version)));
        return Self(std::move(mp), NotRoot{pkg, version});
    }
    static Self no_versions(PId pkg, const Term<V> &term)
    {
        if (term.is_negative())
            throw std::invalid_argument("no_versions must be created with a positive term");

        SmallMap<PId, Term<V>> mp;
        mp.insert(pkg, term);
        return Self(std::move(mp), NoVersions{pkg, term.ranges()});
    }
    static Self custom_term(PId pkg, const Term<V> &term, const M &meta)
    {
        if (!term.is_negative())
            throw std::invalid_argument("custom_term must be created with a negative term");
        SmallMap<PId, Term<V>> mp;
        mp.insert(pkg, term);
        return Self(std::move(mp), Custom{pkg, term.ranges(), meta});
    }
    static Self custom_version(PId pkg, const V &v, const M &meta)
    {
        VS set = Ranges<V>::singleton(v);
        Term<V> t = Term<V>::Positive(set);
        SmallMap<PId, Term<V>> mp;
        mp.insert(pkg, t);
        return Self(std::move(mp), Kind{Custom{pkg, std::move(set), meta}});
    }
    static Self from_dependency(PId pkg, const VS &versions, std::pair<PId, VS> dep)
    {
        auto [p2, set2] = dep;
        SmallMap<PId, Term<V>> mp;
        mp.insert(pkg, Term<V>::Positive(versions));
        if (!set2.is_empty())
            mp.insert(p2, Term<V>::Negative(set2));
        return Self(std::move(mp), FromDependencyOf{pkg, versions, p2, set2});
    }
    std::optional<std::pair<PId, PId>> as_dependency() const
    {
        if (const auto *k = std::get_if<FromDependencyOf>(&kind_))
        {
            return std::make_pair(k->pkg1, k->pkg2);
        }
        return std::nullopt;
    }

    const Term<V> *get(PId p) const
    {
        return terms_.get(p);
    }
    std::size_t size() const { return terms_.len(); }
    auto begin() const { return terms_.begin(); }
    auto end() const { return terms_.end(); }
    std::optional<std::pair<IncompId, IncompId>> causes() const
    {
        if (const auto *k = std::get_if<DerivedFrom>(&kind_))
        {
            return std::make_pair(k->base1.id(), k->base2.id());
        }
        return std::nullopt;
    }

    // a@1とa@2がbに依存しているときに、a@1 || a@2 がbに依存しているものとしてマージする
    std::optional<Self> merge_dependents(const Self &other) const
    {
        auto d1 = this->as_dependency();
        auto d2 = other.as_dependency();
        if (!d1 || !d2)
            return std::nullopt;

        // Both must be the same dependency relationship (same pkg1 -> pkg2)
        if (*d1 != *d2)
            return std::nullopt;

        auto [p1, p2] = *d1;
        if (p1 == p2)
            return std::nullopt;
        auto dep_term = this->get(p2);
        if (!dep_term)
            return std::nullopt;
        // 依存先は同じじゃないとダメ
        if (auto dep_term_other = other.get(p2); !dep_term_other || !(*dep_term == *dep_term_other))
            return std::nullopt;
        const Term<V> *t1 = this->get(p1);
        const Term<V> *t2 = other.get(p1);
        assert(t1 && t2 && t1->is_positive() && t2->is_positive());
        VS merged_ranges = t1->ranges().union_(t2->ranges());
        // 依存先はネガティブタームであるはず(incompatibilityなので)
        VS depSet = dep_term->is_negative() ? dep_term->ranges() : VS::empty();
        return Self::from_dependency(p1, merged_ranges, std::make_pair(p2, depSet));
    }

    static Self prior_cause(IncompId incompat, IncompId satisfier_cause, PId package, const Arena<Self> &store)
    {
        const auto &inc = store[incompat];
        const auto &sat = store[satisfier_cause];

        const Term<V> *t1 = inc.get(package);
        assert(t1);
        SmallMap<PId, Term<V>> merged = inc.terms_;
        for (auto it = sat.begin(); it != sat.end(); ++it)
        {
            PId pk = it->first;
            if (pk == package)
                continue;
            const Term<V> &t2 = it->second;
            if (auto existing = merged.get(pk))
            {
                Term<V> inter = existing->intersection(t2);
                merged.insert(pk, inter);
            }
            else
            {
                merged.insert(pk, t2);
            }
        }
        const Term<V> *t2 = sat.get(package);
        if (t2)
        {
            Term<V> new_term = t1->intersection(*t2);
            if (!(new_term == Term<V>::any()))
            {
                merged.insert(package, new_term);
            }
        }
        return Self(std::move(merged), Kind{DerivedFrom{incompat, satisfier_cause}});
    }
    bool is_terminal(PId root_package, const V &root_version) const
    {
        if (terms_.len() == 0)
            return true;
        if (terms_.len() > 1)
            return false;
        auto it = terms_.begin();
        return it->first == root_package && it->second.contains(root_version);
    }
    template <class TermsFn>
    IncompatRelation<P> relation(TermsFn terms) const
    {
        IncompatRelation<P> rel = IncompatRelation<P>::Satisfied();
        for (auto it = terms_.begin(); it != terms_.end(); ++it)
        {
            PId pkg = it->first;
            const Term<V> &incompat_term = it->second;
            if (const Term<V> *t = terms(pkg))
            {
                auto r = incompat_term.relation_with(*t);
                if (r == ::Relation::Satisfied)
                    continue;
                if (r == ::Relation::Contradicted)
                {
                    return IncompatRelation<P>::Contradicted(pkg);
                }
                else
                {
                    if (rel.tag == IncompatRelationTag::Satisfied)
                    {
                        rel = IncompatRelation<P>::AlmostSatisfied(pkg);
                    }
                    else
                    {
                        return IncompatRelation<P>::Inconclusive();
                    }
                }
            }
            else
            {
                if (rel.tag == IncompatRelationTag::Satisfied)
                {
                    rel = IncompatRelation<P>::AlmostSatisfied(pkg);
                }
                else
                {
                    return IncompatRelation<P>::Inconclusive();
                }
            }
        }
        return rel;
    }

    template <class PackageStore>
    std::string display(const PackageStore &pkgs) const
    {
        std::vector<std::pair<PId, Term<V>>> items;
        items.reserve(terms_.len());
        for (auto it = terms_.begin(); it != terms_.end(); ++it)
        {
            items.emplace_back(it->first, it->second);
        }

        if (items.empty())
        {
            return "version solving failed";
        }

        std::string out;
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (i)
                out += ", ";
            const auto &[p, t] = items[i];
            std::ostringstream oss;
            oss << pkgs[p] << " " << t;
            out += oss.str();
        }
        if (items.size() > 1)
            out += " are incompatible";
        return out;
    }
};
