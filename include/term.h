#pragma once
#include "../include/ranges.h"
#include <ostream>
#include <variant>
#include <vector>

enum class Relation
{
    Satisfied,
    Contradicted,
    Inconclusive
};

template <class T>
class Term
{
public:
    using V = T;

    enum class Polarity
    {
        Positive,
        Negative
    };

private:
    Polarity pol_;
    Ranges<V> ranges_;

public:
    Term() : pol_(Polarity::Positive), ranges_(Ranges<V>::empty()) {}

    static Term any() { return Term(Polarity::Negative, Ranges<V>::full()); }
    static Term empty() { return Term(Polarity::Positive, Ranges<V>::empty()); }
    static Term exact(const V &v) { return Term(Polarity::Positive, Ranges<V>::singleton(v)); }

    static Term Positive(const Ranges<V> &r) { return Term(Polarity::Positive, r); }
    static Term Negative(const Ranges<V> &r) { return Term(Polarity::Negative, r); }

    bool is_positive() const noexcept { return pol_ == Polarity::Positive; }
    bool is_negative() const noexcept { return pol_ == Polarity::Negative; }

    Term negate() const { return Term(is_positive() ? Polarity::Negative : Polarity::Positive, ranges_); }
    bool contains(const V &v) const { return is_positive() ? ranges_.contains(v) : !ranges_.contains(v); }

    Ranges<V> ranges() const { return ranges_; }
    const Ranges<V> &ranges_ref() const { return ranges_; }

    Ranges<V> unwrap_positive() const
    {
        if (!is_positive())
            throw std::runtime_error("Called unwrap_positive on a negative Term");
        return ranges_;
    }
    const Ranges<V> *unwrap_positive_ptr() const
    {
        if (!is_positive())
            return nullptr;
        return &ranges_;
    }
    Ranges<V> unwrap_negative() const
    {
        if (!is_negative())
            throw std::runtime_error("Called unwrap_negative on a positive Term");
        return ranges_;
    }

    Term intersection(const Term &other) const
    {
        if (is_positive() && other.is_positive())
        {
            return Term::Positive(ranges_.intersection(other.ranges_));
        }
        if ((is_positive() && other.is_negative()) ||
            (is_negative() && other.is_positive()))
        {
            const Ranges<V> &p = is_positive() ? ranges_ : other.ranges_;
            const Ranges<V> &n = is_negative() ? ranges_ : other.ranges_;
            return Term::Positive(p.intersection(n.negate()));
        }
        return Negative(ranges_.union_(other.ranges_));
    }

    bool is_disjoint(const Term &other) const
    {
        if (is_positive() && other.is_positive())
        {
            return ranges_.is_disjoint(other.ranges_);
        }
        if (is_negative() && other.is_negative())
        {
            // Negative(r1) ∩ Negative(r2) = complement(r1 ∪ r2)
            // disjoint ⇔ r1 ∪ r2 covers everything (full)
            auto union_ranges = ranges_.union_(other.ranges_);
            return union_ranges.segments().size() == 1 &&
                   union_ranges.segments()[0].first.is_unbounded() &&
                   union_ranges.segments()[0].second.is_unbounded();
        }
        // Positive × Negative case
        const Ranges<V> &p = is_positive() ? ranges_ : other.ranges_;
        const Ranges<V> &n = is_negative() ? ranges_ : other.ranges_;
        // Positive(p) ∩ Negative(n) = p ∩ complement(n)
        // disjoint ⇔ p and complement(n) don't overlap ⇔ p ⊆ n
        return p.subset_of(n);
    }

    Term union_with(const Term &other) const
    {
        if (is_positive() && other.is_positive())
        {
            return Positive(ranges_.union_(other.ranges_));
        }
        if ((is_positive() && other.is_negative()) || (is_negative() && other.is_positive()))
        {
            const Ranges<V> &p = is_positive() ? ranges_ : other.ranges_;
            const Ranges<V> &n = is_negative() ? ranges_ : other.ranges_;
            return Negative(p.negate().intersection(n));
        }
        // Negative × Negative: union of complements = Negative(intersection of complements)
        // complement(r1) ∪ complement(r2) represented as Negative(r)
        // where r = complement(r1) ∩ complement(r2) = r1.negate() ∩ r2.negate()
        return Negative(ranges_.negate().intersection(other.ranges_.negate()));
    }

    bool subset_of(const Term &other) const
    {
        if (is_positive() && other.is_positive())
        {
            return ranges_.subset_of(other.ranges_);
        }
        if (is_positive() && other.is_negative())
        {
            return ranges_.is_disjoint(other.ranges_);
        }
        if (is_negative() && other.is_positive())
        {
            return false;
        }
        return other.ranges_.subset_of(ranges_);
    }

    Relation relation_with(const Term &other_terms_interction) const
    {
        if (other_terms_interction.subset_of(*this))
        {
            return Relation::Satisfied;
        }
        else if (this->is_disjoint(other_terms_interction))
        {
            return Relation::Contradicted;
        }
        else
        {
            return Relation::Inconclusive;
        }
    }
    friend std::ostream &operator<<(std::ostream &os, const Term &t)
    {
        if (t.is_positive())
        {
            return os << t.ranges_;
        }
        else
        {
            return os << "Not(" << t.ranges_ << ")";
        }
    }

    friend bool operator==(const Term &a, const Term &b)
    {
        if (a.is_positive() != b.is_positive())
            return false;
        return a.ranges_ == b.ranges_;
    }
    friend bool operator!=(const Term &a, const Term &b) { return !(a == b); }

private:
    Term(Polarity p, const Ranges<V> &r) : pol_(p), ranges_(r) {}
};