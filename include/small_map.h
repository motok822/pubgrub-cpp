#pragma once
#include <variant>
#include <vector>
#include <utility>
#include <cassert>
#include <optional>
#include <functional>
#include <span>
#include <unordered_map>
#include <stdexcept>

template <class T>
class SmallVec
{
    using V = std::variant<std::monostate, std::array<T, 1>, std::array<T, 2>, std::vector<T>>;
    using const_iterator = const T *;
    using iterator = T *;
    V data_;

public:
    SmallVec() : data_(std::monostate{}) {}
    SmallVec empty()
    {
        return SmallVec();
    }

    SmallVec one(const T &value)
    {
        SmallVec sv;
        sv.data_ = std::array<T, 1>{value};
        return sv;
    }

    void push(const T &value)
    {
        switch (data_.index())
        {
        case 0:
            data_ = std::array<T, 1>{value};
            return;
        case 1:
        {
            auto a1 = std::move(std::get<1>(data_));
            data_ = std::array<T, 2>{std::move(a1[0]), value};
            return;
        }
        case 2:
        {
            auto a2 = std::move(std::get<2>(data_));
            std::vector<T> vec;
            vec.reserve(3);
            vec.push_back(std::move(a2[0]));
            vec.push_back(std::move(a2[1]));
            vec.push_back(value);
            data_ = std::move(vec);
            return;
        }
        case 3:
            std::get<3>(data_).push_back(value);
            return;
        default:
            assert(false && "unreachable");
        }
    }

    void clear()
    {
        if (data_.index() == 3)
            std::get<3>(data_).clear();
        else
            data_ = std::monostate{};
    }

    std::span<const T> view() const noexcept
    {
        switch (data_.index())
        {
        case 0:
            return {};
        case 1:
        {
            auto &a1 = std::get<1>(data_);
            return std::span<const T>(a1.data(), 1);
        }
        case 2:
        {
            auto &a2 = std::get<2>(data_);
            return std::span<const T>(a2.data(), 2);
        }
        case 3:
        {
            auto &vc = std::get<3>(data_);
            return std::span<const T>(vc.data(), vc.size());
        }
        }
        return {};
    }

    std::span<T> view() noexcept
    {
        switch (data_.index())
        {
        case 0:
            return {};
        case 1:
        {
            auto &a1 = std::get<1>(data_);
            return std::span<T>(a1.data(), 1);
        }
        case 2:
        {
            auto &a2 = std::get<2>(data_);
            return std::span<T>(a2.data(), 2);
        }
        case 3:
        {
            auto &v = std::get<3>(data_);
            return std::span<T>(v.data(), v.size());
        }
        }
        return {};
    }
    struct Hasher
    {
        std::size_t operator()(const SmallVec &s) const noexcept
        {
            std::size_t h = std::hash<std::size_t>{}(s.size());
            for (auto const &x : s.view())
            {
                std::size_t hx = std::hash<std::decay_t<decltype(x)>>{}(x);
                h ^= hx + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            }
            return h;
        }
    };

    const T *data() const noexcept
    {
        return view().data();
    }
    T *data() noexcept { return view().data(); }

    std::size_t size() const { return view().size(); }
    bool empty_vec() const { return size() == 0; }
    const T &operator[](std::size_t i) const { return view()[i]; }
    T &operator[](std::size_t i) { return view()[i]; }

    const_iterator begin() const noexcept { return view().data(); }
    const_iterator end() const noexcept
    {
        auto v = view();
        return v.data() + v.size();
    }
    iterator begin() noexcept { return view().data(); }
    iterator end() noexcept
    {
        auto v = view();
        return v.data() + v.size();
    }
};

template <class K, class V, class Hash = std::hash<K>, class Eq = std::equal_to<K>>
class SmallMap
{
    using P1 = std::array<std::pair<const K, V>, 1>;
    using P2 = std::array<std::pair<const K, V>, 2>;
    using Map = std::unordered_map<K, V, Hash, Eq>;
    using Var = std::variant<std::monostate, P1, P2, Map>;
    Var data_;

public:
    SmallMap() : data_(std::monostate{}) {}
    std::size_t len() const
    {
        switch (data_.index())
        {
        case 0:
            return 0;
        case 1:
            return 1;
        case 2:
            return 2;
        case 3:
            return std::get<3>(data_).size();
        default:
            assert(false && "unreachable");
            return 0;
        }
    }
    const V *get(const K &key) const
    {
        switch (data_.index())
        {
        case 0:
            return nullptr;
        case 1:
        {
            auto const &a = std::get<1>(data_);
            return Eq{}(a[0].first, key) ? &a[0].second : nullptr;
        }
        case 2:
        {
            auto const &b = std::get<2>(data_);
            if (Eq{}(b[0].first, key))
                return &b[0].second;
            if (Eq{}(b[1].first, key))
                return &b[1].second;
            return nullptr;
        }
        case 3:
        {
            auto const &m = std::get<3>(data_);
            auto it = m.find(key);
            if (it != m.end())
                return &it->second;
            return nullptr;
        }
        }
        return nullptr;
    }

    void insert(const K &key, const V &value)
    {
        switch (data_.index())
        {
        case 0:
            data_.template emplace<P1>(P1{std::pair<const K, V>(key, value)});
            break;
        case 1:
            if (Eq{}(std::get<1>(data_)[0].first, key))
            {
                std::get<1>(data_)[0].second = value;
                return;
            }
            data_.template emplace<P2>(
                P2{std::pair<const K, V>(std::get<1>(data_)[0].first, std::move(std::get<1>(data_)[0].second)),
                   std::pair<const K, V>(key, value)});
            break;
        case 2:
            if (Eq{}(std::get<2>(data_)[0].first, key))
            {
                std::get<2>(data_)[0].second = value;
                return;
            }
            if (Eq{}(std::get<2>(data_)[1].first, key))
            {
                std::get<2>(data_)[1].second = value;
                return;
            }
            {
                Map m;
                m.emplace(std::get<2>(data_)[0].first, std::move(std::get<2>(data_)[0].second));
                m.emplace(std::get<2>(data_)[1].first, std::move(std::get<2>(data_)[1].second));
                m.emplace(key, value);
                data_ = std::move(m);
            }
            break;
        case 3:
            std::get<3>(data_)[key] = value;
            break;
        default:
            assert(false && "unreachable case in insert");
        }
    }
    void remove(const K &key)
    {
        switch (data_.index())
        {
        case 0:
            return;
        case 1:
            if (Eq{}(std::get<1>(data_)[0].first, key))
                data_ = std::monostate{};
            return;
        case 2:
            if (Eq{}(std::get<2>(data_)[0].first, key))
            {
                // 最初の要素を削除、2つ目の要素だけ残す
                data_.template emplace<P1>(P1{std::pair<const K, V>(std::get<2>(data_)[1].first, std::move(std::get<2>(data_)[1].second))});
                return;
            }
            if (Eq{}(std::get<2>(data_)[1].first, key))
            {
                // 2つ目の要素を削除、最初の要素だけ残す
                data_.template emplace<P1>(P1{std::pair<const K, V>(std::get<2>(data_)[0].first, std::move(std::get<2>(data_)[0].second))});
                return;
            }
            return;
        case 3:
            std::get<3>(data_).erase(key);
            return;
        default:
            assert(false && "unreachable case in remove");
        }
    }
    void merge(const SmallMap &other, std::function<V(const V &, const V &)> merge_values)
    {
        for (const auto &[k, v] : other)
        {
            if (const V *existing = this->get(k))
            {
                V merged = merge_values(*existing, v);
                this->insert(k, merged);
            }
            else
            {
                this->insert(k, v);
            }
        }
    }
    std::optional<V> split_one(const K &key)
    {
        switch (data_.index())
        {
        case 0:
            return std::nullopt;
        case 1:
            if (Eq{}(std::get<1>(data_)[0].first, key))
            {
                V value = std::move(std::get<1>(data_)[0].second);
                data_ = std::monostate{};
                return value;
            }
            return std::nullopt;
        case 2:
            if (Eq{}(std::get<2>(data_)[0].first, key))
            {
                V value = std::move(std::get<2>(data_)[0].second);
                data_.template emplace<P1>(P1{std::pair<const K, V>(std::get<2>(data_)[1].first, std::move(std::get<2>(data_)[1].second))});
                return value;
            }
            if (Eq{}(std::get<2>(data_)[1].first, key))
            {
                V value = std::move(std::get<2>(data_)[1].second);
                data_ = std::monostate{};
                return value;
            }
            return std::nullopt;
        case 3:
            if (auto it = std::get<3>(data_).find(key); it != std::get<3>(data_).end())
            {
                V value = std::move(it->second);
                std::get<3>(data_).erase(it);
                return value;
            }
            return std::nullopt;
        default:
            assert(false && "unreachable case in split_one");
        }
    }
    // Custom iterator that works for all modes
    class Iterator
    {
    private:
        std::variant<
            std::monostate,
            std::pair<const K, V> *,
            typename Map::iterator>
            iter_;
        std::pair<const K, V> *end_ptr_;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const K, V>;
        using difference_type = std::ptrdiff_t;
        using pointer = std::pair<const K, V> *;
        using reference = std::pair<const K, V> &;

        Iterator() : iter_(std::monostate{}), end_ptr_(nullptr) {}
        Iterator(std::pair<const K, V> *ptr, std::pair<const K, V> *end) : iter_(ptr), end_ptr_(end) {}
        Iterator(typename Map::iterator it) : iter_(it), end_ptr_(nullptr) {}

        reference operator*() const
        {
            if (auto *ptr = std::get_if<1>(&iter_))
                return (**ptr);
            else if (auto *it = std::get_if<2>(&iter_))
                return *(*it);
            throw std::runtime_error("Invalid iterator dereference");
        }

        pointer operator->() const
        {
            return &(**this);
        }

        Iterator &operator++()
        {
            if (auto *ptr = std::get_if<1>(&iter_))
                ++(*ptr);
            else if (auto *it = std::get_if<2>(&iter_))
                ++(*it);
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const Iterator &a, const Iterator &b)
        {
            if (a.iter_.index() != b.iter_.index())
                return false;
            if (auto *ptr = std::get_if<1>(&a.iter_))
                return *ptr == *std::get_if<1>(&b.iter_);
            else if (auto *it = std::get_if<2>(&a.iter_))
                return *it == *std::get_if<2>(&b.iter_);
            return true;
        }

        friend bool operator!=(const Iterator &a, const Iterator &b)
        {
            return !(a == b);
        }
    };

    class ConstIterator
    {
    private:
        std::variant<
            std::monostate,
            const std::pair<const K, V> *,
            typename Map::const_iterator>
            iter_;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const K, V>;
        using difference_type = std::ptrdiff_t;
        using pointer = const std::pair<const K, V> *;
        using reference = const std::pair<const K, V> &;

        ConstIterator() : iter_(std::monostate{}) {}
        ConstIterator(const std::pair<const K, V> *ptr) : iter_(ptr) {}
        ConstIterator(typename Map::const_iterator it) : iter_(it) {}

        reference operator*() const
        {
            if (auto *ptr = std::get_if<1>(&iter_))
            {
                return (**ptr);
            }
            else if (auto *it = std::get_if<2>(&iter_))
            {
                return *(*it);
            }
            throw std::runtime_error("Invalid iterator dereference");
        }

        pointer operator->() const
        {
            return &(**this);
        }

        ConstIterator &operator++()
        {
            if (auto *ptr = std::get_if<1>(&iter_))
            {
                ++(*ptr);
            }
            else if (auto *it = std::get_if<2>(&iter_))
            {
                ++(*it);
            }
            return *this;
        }

        ConstIterator operator++(int)
        {
            ConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const ConstIterator &a, const ConstIterator &b)
        {
            if (a.iter_.index() != b.iter_.index())
                return false;
            if (auto *ptr = std::get_if<1>(&a.iter_))
            {
                return *ptr == *std::get_if<1>(&b.iter_);
            }
            else if (auto *it = std::get_if<2>(&a.iter_))
            {
                return *it == *std::get_if<2>(&b.iter_);
            }
            return true;
        }

        friend bool operator!=(const ConstIterator &a, const ConstIterator &b)
        {
            return !(a == b);
        }
    };

    using iterator = Iterator;
    using const_iterator = ConstIterator;

    iterator begin()
    {
        switch (data_.index())
        {
        case 0:
            return Iterator();
        case 1:
        {
            auto &a = std::get<1>(data_);
            return Iterator(a.data(), a.data() + 1);
        }
        case 2:
        {
            auto &a = std::get<2>(data_);
            return Iterator(a.data(), a.data() + 2);
        }
        case 3:
            return Iterator(std::get<3>(data_).begin());
        default:
            return Iterator();
        }
    }

    iterator end()
    {
        switch (data_.index())
        {
        case 0:
            return Iterator();
        case 1:
        {
            auto &a = std::get<1>(data_);
            return Iterator(a.data() + 1, a.data() + 1);
        }
        case 2:
        {
            auto &a = std::get<2>(data_);
            return Iterator(a.data() + 2, a.data() + 2);
        }
        case 3:
            return Iterator(std::get<3>(data_).end());
        default:
            return Iterator();
        }
    }

    const_iterator begin() const
    {
        switch (data_.index())
        {
        case 0:
            return ConstIterator();
        case 1:
            return ConstIterator(std::get<1>(data_).data());
        case 2:
            return ConstIterator(std::get<2>(data_).data());
        case 3:
            return ConstIterator(std::get<3>(data_).begin());
        default:
            return ConstIterator();
        }
    }

    const_iterator end() const
    {
        switch (data_.index())
        {
        case 0:
            return ConstIterator();
        case 1:
            return ConstIterator(std::get<1>(data_).data() + 1);
        case 2:
            return ConstIterator(std::get<2>(data_).data() + 2);
        case 3:
            return ConstIterator(std::get<3>(data_).end());
        default:
            return ConstIterator();
        }
    }
};