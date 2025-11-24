#pragma once
#include <vector>
#include <utility>
#include <iostream>
#include <algorithm>
#include <optional>
#include <functional>
#include <type_traits>
#include <cassert>

enum class Inclusivity
{
    Open,
    Closed
};

template <class V>
class Bound
{
public:
    enum class Kind
    {
        Unbounded,
        Finite,
    };

    Kind kind = Kind::Unbounded;
    V value{};
    Inclusivity inc = Inclusivity::Open;

    Bound(Kind k, const V &val, Inclusivity i) : kind(k), value(val), inc(i) {}
    Bound() : kind(Kind::Unbounded), value(), inc(Inclusivity::Open) {}
    static Bound<V> unbounded() { return Bound<V>(); }
    static Bound<V> closed(const V &val) { return Bound<V>(Kind::Finite, val, Inclusivity::Closed); }
    static Bound<V> open(const V &val) { return Bound<V>(Kind::Finite, val, Inclusivity::Open); }
    static Bound<V> flip_inclusivity(const Bound<V> &b)
    {
        if (b.is_unbounded())
            return b;
        return Bound<V>(Kind::Finite, b.value, b.is_closed() ? Inclusivity::Open : Inclusivity::Closed);
    }

    bool is_unbounded() const { return kind == Kind::Unbounded; }
    bool is_finite() const { return kind == Kind::Finite; }
    bool is_closed() const { return inc == Inclusivity::Closed; }
    bool is_open() const { return inc == Inclusivity::Open; }

    bool operator==(const Bound<V> &other) const
    {
        if (kind != other.kind)
            return false;
        if (is_unbounded())
            return true;
        return value == other.value && inc == other.inc;
    }

    bool operator!=(const Bound<V> &other) const
    {
        return !(*this == other);
    }
};

template <class V>
using Interval = std::pair<Bound<V>, Bound<V>>;

template <class V>
class Ranges
{
public:
    using B = Bound<V>;
    using Segments = std::vector<Interval<V>>;

    Ranges() = default;
    static Ranges<V> empty() { return Ranges<V>(); }
    static Ranges<V> full()
    {
        Ranges<V> r;
        r.segments_.push_back({B::unbounded(), B::unbounded()});
        return r;
    }
    static Ranges higher_than(const V &value)
    {
        Ranges<V> r;
        r.segments_.push_back({B::closed(value), B::unbounded()});
        return r;
    }
    static Ranges strictly_higher_than(const V &value)
    {
        Ranges<V> r;
        r.segments_.push_back({B::open(value), B::unbounded()});
        return r;
    }
    static Ranges lower_than(const V &value)
    {
        Ranges<V> r;
        r.segments_.push_back({B::unbounded(), B::closed(value)});
        return r;
    }
    static Ranges strictly_lower_than(const V &value)
    {
        Ranges<V> r;
        r.segments_.push_back({B::unbounded(), B::open(value)});
        return r;
    }
    static Ranges between(const V &low, const V &high)
    {
        Ranges<V> r;
        r.segments_.push_back({B::closed(low), B::open(high)});
        return r;
    }
    bool is_empty() const
    {
        return segments_.size() == 0;
    }
    static Ranges<V> singleton(const V &value)
    {
        Ranges<V> r;
        r.segments_.push_back({B::closed(value), B::closed(value)});
        return r;
    }
    Ranges<V> complement() const
    {
        if (segments_.empty())
        {
            return Ranges<V>::full();
        }
        if (segments_.size() == 1 &&
            segments_[0].first.is_unbounded() &&
            segments_[0].second.is_unbounded())
        {
            return Ranges<V>::empty();
        }

        Ranges<V> result;
        auto current = B::unbounded();

        for (const auto &[start, end] : segments_)
        {
            // Check if there's a gap to fill with complement
            bool has_gap = false;
            if (current.is_unbounded() && start.is_finite())
            {
                has_gap = true;
            }
            else if (current.is_finite() && start.is_finite())
            {
                has_gap = end_before_start_with_gap(current, start);
            }

            if (has_gap)
            {
                result.segments_.push_back({current, B::flip_inclusivity(start)});
            }

            current = B::flip_inclusivity(end);
        }

        if (current.is_finite())
        {
            result.segments_.push_back({current, B::unbounded()});
        }

        return result;
    }

    Ranges<V> negate() const
    {
        return complement();
    }

    const Segments &segments() const { return segments_; }

    Ranges<V> union_(const Ranges<V> &other) const
    {
        if (segments_.empty())
            return other;
        if (other.segments_.empty())
            return *this;

        Ranges<V> result;
        auto left_iter = segments_.begin();
        auto right_iter = other.segments_.begin();
        auto left_end = segments_.end();
        auto right_end = other.segments_.end();

        std::optional<Interval<V>> current;

        while (left_iter != left_end && right_iter != right_end)
        {
            const auto &left_interval = *left_iter;
            const auto &right_interval = *right_iter;

            Interval<V> next_interval;
            if (left_start_is_smaller(left_interval.first, right_interval.first))
            {
                next_interval = left_interval;
                ++left_iter;
            }
            else
            {
                next_interval = right_interval;
                ++right_iter;
            }

            if (!current.has_value())
            {
                current = next_interval;
            }
            else
            {
                if (end_before_start_with_gap(current->second, next_interval.first))
                {
                    result.segments_.push_back(*current);
                    current = next_interval;
                }
                else
                {
                    if (left_end_is_smaller(current->second, next_interval.second))
                    {
                        current->second = next_interval.second;
                    }
                }
            }
        }

        auto process_remaining = [&](auto iter, auto end)
        {
            while (iter != end)
            {
                const auto &interval = *iter;
                if (!current.has_value())
                {
                    current = interval;
                }
                else
                {
                    if (end_before_start_with_gap(current->second, interval.first))
                    {
                        result.segments_.push_back(*current);
                        current = interval;
                    }
                    else
                    {
                        if (left_end_is_smaller(current->second, interval.second))
                        {
                            current->second = interval.second;
                        }
                    }
                }
                ++iter;
            }
        };

        process_remaining(left_iter, left_end);
        process_remaining(right_iter, right_end);

        if (current.has_value())
        {
            result.segments_.push_back(*current);
        }

        return result;
    }

    Ranges<V> intersection(const Ranges<V> &other) const
    {
        if (segments_.empty() || other.segments_.empty())
            return Ranges<V>::empty();

        Ranges<V> result;
        auto left_iter = segments_.begin();
        auto right_iter = other.segments_.begin();
        auto left_end = segments_.end();
        auto right_end = other.segments_.end();

        while (left_iter != left_end && right_iter != right_end)
        {
            const auto &[left_start, left_end_bound] = *left_iter;
            const auto &[right_start, right_end_bound] = *right_iter;

            auto start = left_start_is_smaller(left_start, right_start) ? right_start : left_start;
            auto end = left_end_is_smaller(left_end_bound, right_end_bound) ? left_end_bound : right_end_bound;

            if (valid_segment(start, end))
            {
                result.segments_.push_back({start, end});
            }

            if (left_end_is_smaller(left_end_bound, right_end_bound))
            {
                ++left_iter;
            }
            else
            {
                ++right_iter;
            }
        }

        return result;
    }

    bool contains(const V &version) const
    {
        for (const auto &[start, end] : segments_)
        {
            if (start.is_finite() && start.is_closed() && version < start.value)
                continue;
            if (start.is_finite() && start.is_open() && version <= start.value)
                continue;
            if (end.is_finite() && end.is_closed() && version > end.value)
                continue;
            if (end.is_finite() && end.is_open() && version >= end.value)
                continue;
            return true;
        }
        return false;
    }

    bool is_disjoint(const Ranges<V> &other) const
    {
        return intersection(other).is_empty();
    }

    bool subset_of(const Ranges<V> &other) const
    {
        return intersection(other).segments_ == segments_;
    }

    std::optional<V> as_singleton() const
    {
        if (segments_.size() == 1)
        {
            const auto &[start, end] = segments_[0];
            if (start.is_finite() && end.is_finite() &&
                start.is_closed() && end.is_closed() &&
                start.value == end.value)
            {
                return start.value;
            }
        }
        return std::nullopt;
    }

    bool operator==(const Ranges<V> &other) const
    {
        return segments_ == other.segments_;
    }

    bool operator!=(const Ranges<V> &other) const
    {
        return segments_ != other.segments_;
    }

    friend std::ostream &operator<<(std::ostream &os, const Ranges<V> &r)
    {
        if (r.segments_.empty())
        {
            return os << "{}";
        }
        if (r.segments_.size() == 1 &&
            r.segments_[0].first.is_unbounded() &&
            r.segments_[0].second.is_unbounded())
        {
            return os << "(-∞, +∞)";
        }

        bool first = true;
        for (const auto &[start, end] : r.segments_)
        {
            if (!first)
                os << " ∪ ";
            first = false;

            // Print start bound
            if (start.is_unbounded())
                os << "(-∞";
            else if (start.is_closed())
                os << "[" << start.value;
            else
                os << "(" << start.value;

            os << ", ";

            // Print end bound
            if (end.is_unbounded())
                os << "+∞)";
            else if (end.is_closed())
                os << end.value << "]";
            else
                os << end.value << ")";
        }
        return os;
    }

private:
    Segments segments_;
};

template <class V>
bool valid_segment(const Bound<V> &start, const Bound<V> &end)
{
    if (start.is_unbounded() || end.is_unbounded())
        return true;
    if (start.value < end.value)
        return true;
    if (start.value == end.value && start.is_closed() && end.is_closed())
        return true;
    return false;
}
/// True for these two:
///  |----|
///                |-----|
///       ^ end    ^ start
/// False for these two:
///  |----|
///     |-----|
/// Here it depends: If they both exclude the position they share, there is a version in between
/// them that blocks concatenation
///  |----|
///       |-----|
/// ```
template <class V>
bool end_before_start_with_gap(const Bound<V> &end, const Bound<V> &start)
{
    if (end.is_unbounded() || start.is_unbounded())
        return false;
    if (end.value < start.value)
        return true;
    if (end.value == start.value && (end.is_open() || start.is_open()))
        return true;
    return false;
}

template <class V>
bool left_start_is_smaller(const Bound<V> &left, const Bound<V> &right)
{
    if (left.is_unbounded())
        return true;
    if (right.is_unbounded())
        return false;
    if (left.value < right.value)
        return true;
    if (left.value == right.value && left.is_closed() && right.is_open())
        return true;
    return false;
}

template <class V>
bool left_end_is_smaller(const Bound<V> &left, const Bound<V> &right)
{
    if (right.is_unbounded())
        return true;
    if (left.is_unbounded())
        return false;
    if (left.value < right.value)
        return true;
    if (left.value == right.value && left.is_open() && right.is_closed())
        return true;
    return false;
}