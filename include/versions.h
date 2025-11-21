#pragma once
#include <concepts>
#include <type_traits>

template <class S>
concept VersionSet = requires(const S &a, const S &b, const typename S::value_type &v) {
    typename S::value_type;
    { S::empty() } -> std::same_as<S>;
    { S::singleton(v) } -> std::same_as<S>;
    { a.complement() } -> std::same_as<S>;
    { a.intersection(b) } -> std::same_as<S>;
    { a.contains(v) } -> std::same_as<bool>;

    { a == b } -> std::convertible_to<bool>;
    { a != b } -> std::convertible_to<bool>;
};
template <VersionSet S>
[[nodiscard]] inline S vs_full()
{
    return S::full();
}
template <VersionSet S>
[[nodiscard]] inline S vs_union(const S &lhs, const S &rhs)
{
    // De Morgan: A ∪ B = (Aᶜ ∩ Bᶜ)ᶜ
    return lhs.complement().intersection(rhs.complement()).complement();
}

template <VersionSet S>
[[nodiscard]] inline bool vs_is_disjoint(const S &lhs, const S &rhs)
{
    return lhs.intersection(rhs) == S::empty();
}

template <VersionSet S>
[[nodiscard]] inline bool vs_subset_of(const S &lhs, const S &rhs)
{
    return lhs == lhs.intersection(rhs);
}
