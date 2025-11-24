#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cassert>
#include <typeinfo>
#include <iostream>

template <class T>
struct Id
{
    uint32_t raw{0};

    constexpr Id() = default;
    constexpr explicit Id(uint32_t r) : raw(r) {}

    friend constexpr bool operator==(const Id &a, const Id &b) { return a.raw == b.raw; }
    friend constexpr bool operator!=(const Id &a, const Id &b) { return a.raw != b.raw; }
    friend constexpr bool operator<(const Id &a, const Id &b) { return a.raw < b.raw; }

    constexpr std::size_t into_raw() const { return static_cast<std::size_t>(raw); }
    friend std::ostream &operator<<(std::ostream &os, const Id &x)
    {
        return os << "Id<" << typeid(T).name() << ">(" << x.raw << ")";
    }
    static constexpr Id from(uint32_t r) { return Id(r); }
};

// Specialize std::hash for Id<T>
namespace std
{
    template <class T>
    struct hash<Id<T>>
    {
        std::size_t operator()(const Id<T> &s) const noexcept
        {
            return std::hash<uint32_t>{}(s.raw);
        }
    };
}
template <class T>
struct IdRange
{
    Id<T> start{};
    Id<T> end_{};

    struct Iterator
    {
        uint32_t cur;
        uint32_t last;
        bool operator!=(const Iterator &other) const { return cur != other.cur; }
        void operator++() { ++cur; }
        Id<T> operator*() const { return Id<T>::from(cur); }
    };
    Iterator begin() const { return Iterator{start.raw, end_.raw}; }
    Iterator end() const { return Iterator{end_.raw, end_.raw}; }
};

template <class T>
class Arena
{
    std::vector<T> data_;

public:
    Arena() = default;

    Id<T> alloc(const T &value)
    {
        data_.push_back(value);
        return Id<T>::from(static_cast<uint32_t>(data_.size() - 1));
    }
    template <class It>
    IdRange<T> alloc_range(It begin, It end)
    {
        auto start = Id<T>::from(static_cast<uint32_t>(data_.size()));
        for (; begin != end; ++begin)
        {
            alloc(*begin);
        }
        auto end_id = Id<T>::from(static_cast<uint32_t>(data_.size()));
        return IdRange<T>{start, end_id};
    }
    const T &operator[](Id<T> id) const
    {
        auto i = id.into_raw();
        assert(i < data_.size());
        return data_[i];
    }
    T &operator[](Id<T> id)
    {
        auto i = id.into_raw();
        assert(i < data_.size());
        return data_[i];
    }
    const std::vector<T> slice(IdRange<T> r) const
    {
        auto s = r.start.into_raw();
        auto e = r.end.into_raw();
        assert(s <= e && e <= data_.size());
        return std::vector<T>(data_.begin() + s, data_.begin() + e);
    }
    std::size_t size() const { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }
    friend std::ostream &operator<<(std::ostream &os, const Arena &a)
    {
        os << "Arena<" << typeid(T).name() << ">{size=" << a.size() << "}";
        return os;
    }
};

template <class T, class Hasher = std::hash<T>, class Eq = std::equal_to<T>>
class HashArena
{
    std::vector<T> data_;
    std::unordered_map<T, std::size_t, Hasher, Eq> index_map_;

public:
    HashArena() = default;
    Id<T> alloc(const T &value)
    {
        if (auto it = index_map_.find(value); it != index_map_.end())
        {
            return Id<T>::from(static_cast<uint32_t>(it->second));
        }
        std::size_t pos = data_.size();
        data_.push_back(value);
        index_map_.emplace(data_.back(), pos);
        return Id<T>::from(static_cast<uint32_t>(pos));
    }

    Id<T> alloc(T &&value)
    {
        if (auto it = index_map_.find(value); it != index_map_.end())
        {
            return Id<T>::from(static_cast<uint32_t>(it->second));
        }
        std::size_t pos = data_.size();
        data_.push_back(std::move(value));
        index_map_.emplace(data_.back(), pos);
        return Id<T>::from(static_cast<uint32_t>(pos));
    }
    const T &operator[](Id<T> id) const
    {
        auto i = id.into_raw();
        assert(i < data_.size());
        return data_[i];
    }
    std::size_t size() const { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }
    friend std::ostream &operator<<(std::ostream &os, const HashArena &a)
    {
        os << "HashArena<" << typeid(T).name() << ">{size=" << a.size() << "}";
        return os;
    }
};