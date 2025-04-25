#ifndef SUFFIX_TRIE_H
#define SUFFIX_TRIE_H

#include <iostream>
#include <memory>
#include <ranges>
#include <vector>

#include "art.h"

// NOLINTBEGIN

namespace suffix_trie {

// Suffix leaf holds either value or a vector of pointers to other leaves. Node that single suffix
// leaf can be both a terminal node holding a value, and a suffix node which holds pointers to
// terminal leaves.
//
template<class T = void*>
class Suffix_leaf_2 {
public:
    auto& leaves() noexcept { return m_leaf_ptrs; }

    const auto& leaves() const noexcept { return m_leaf_ptrs; }

    // Only include parts not included in ART size_in_bytes.
    //
    const size_t size_in_bytes() const noexcept
    {
        return (m_value ? sizeof(T) : 0) +
               m_leaf_ptrs.size() * sizeof(std::vector<typename art::ART<Suffix_leaf_2>::Leaf*>);
    }

private:
    std::vector<typename art::ART<Suffix_leaf_2>::Leaf*> m_leaf_ptrs;
};

// Suffix trie. It is a key/value container which holds full key (string by default) which holds
// value (T) and all suffixes of a key which points to a full key. This provides the ability to
// search suffix strings instead of just exact key search. Combined with ART::prefix_search, this
// gives us powerfull search possibilities.
//
template<class T = void*>
class Suffix_trie_2 : public art::ART<Suffix_leaf_2<T>> {
public:
    using Suffix_leaf_2 = Suffix_leaf_2<T>;
    using ART = art::ART<Suffix_leaf_2>;
    using Leaf = ART::Leaf;

    // Class that wraps insert result.
    // It holds a pointer to Leaf and a bool flag representing whether insert succeeded (read insert
    // for more details).
    //
    class result {
    public:
        result(Suffix_leaf_2* value, bool ok) : m_value{value}, m_ok{ok}
        {
            assert(m_value != nullptr);
        }

        Suffix_leaf_2* get() noexcept { return m_value; }

        const Suffix_leaf_2* get() const noexcept { return m_value; }

        constexpr bool ok() const noexcept { return m_ok; }

        Suffix_leaf_2* operator->() noexcept { return get(); }

        const Suffix_leaf_2* operator->() const noexcept { return get(); }

        constexpr operator bool() const noexcept { return ok(); }

    private:
        Suffix_leaf_2* m_value;
        bool m_ok;
    };

    // Inserts full suffix string (holding T value) and all of it's suffixes which will point to
    // full suffix.
    //
    template<class V = T>
    result insert_suffix(const std::string& suffix, V&& value = V{})
    {
        auto r = ART::insert(suffix, {}); // TODO: Implement emplace in ART and avoid this nonsence.

        auto& value_ptr = r->value().ptr();
        if (value_ptr) // Suffix already exist. Let caller decide what to do.
            return {&r->value(), false};

        value_ptr = std::make_unique<T>(std::forward<V>(value));
        Leaf* leaf = r.get();

        uint8_t* begin = (uint8_t*)suffix.data();
        uint8_t* end = begin + suffix.size();

        while (++begin <= end) {
            // TODO: Implement emplace in ART and avoid this nonsence.
            auto res = ART::insert(begin, end - begin, {});
            auto& leaves = res->value().leaves();

            if (res) // Vector entry does not exist, so just push back new pointer.
                leaves.push_back(leaf);
            else if (std::ranges::find(leaves, leaf) == leaves.end())
                leaves.push_back(leaf);
        }

        return result{&r->value(), true};
    }

    // Erase full string with value and all of it's suffixes.
    //
    void erase_suffix(const std::string& suffix)
    {
        Leaf* main_leaf = ART::search(suffix);
        if (!main_leaf)
            return;

        auto& value_ptr = main_leaf->value().ptr();
        if (!value_ptr) // Check if there is a value in this leaf.
            return;

        uint8_t* suffix_start = (uint8_t*)suffix.data();
        uint8_t* end = suffix_start + suffix.size();
        uint8_t* start = end;

        while (start > suffix_start) {
            Leaf* leaf = ART::search(start, end - start);
            assert(leaf != nullptr);

            auto& ptrs = leaf->value().leaves();
            assert(std::ranges::find(ptrs, main_leaf) != ptrs.end());

            std::erase(ptrs, main_leaf);
            if (ptrs.empty() && !leaf->value().ptr())
                ART::erase(start, end - start);

            --start;
        }

        value_ptr.reset();
        if (main_leaf->value().leaves().empty())
            ART::erase(suffix);
    }

    // Returns a vector of T* where key matches str suffix.
    //
    [[nodiscard]] auto search_suffix(const std::string& str) const noexcept
    {
        std::vector<const T*> results;

        if (Leaf* leaf = ART::search(str)) {
            T* value = leaf->value().ptr().get();
            if (value)
                results.push_back(value);

            for (Leaf* leaf_ptr : leaf->value().leaves()) {
                value = leaf_ptr->value().ptr().get();
                assert(value);
                if (value && std::ranges::find(results, value) == results.end())
                    results.push_back(value);
            }
        }

        return results;
    }

    // Returns a vector of T* where key matches str prefix.
    //
    [[nodiscard]] auto search_prefix(const std::string& str) const noexcept
    {
        std::vector<const T*> results;

        const auto& leaves = ART::search_prefix(str);
        for (const auto& leaf : leaves) {
            T* value = leaf->value().ptr().get();
            if (value && std::ranges::find(results, value) == results.end())
                results.push_back(value);

            for (Leaf* leaf_ptr : leaf->value().leaves()) {
                value = leaf_ptr->value().ptr().get();
                assert(value);
                if (std::ranges::find(results, value) == results.end())
                    results.push_back(value);
            }
        }

        return results;
    }

    void print_links()
    {
        size_t dist[512] = {};
        ART::for_each_leaf([&](const ART::Leaf* leaf) {
            const Suffix_leaf_2& sl = leaf->value();
            auto& leaves = sl.leaves();

            if (leaves.size() >= 512)
                std::cout << "Links number too big: " << leaves.size()
                          << ". Value: " << leaf->key_to_string() << "\n";
            else
                ++dist[leaves.size()];
        });

        for (int i = 0; i < 512; ++i)
            std::cout << i << ": " << dist[i] << "\n";
    }

    size_t size_in_bytes(bool full_leaves = true)
    {
        size_t suff_size = 0;
        ART::for_each_leaf(
            [&](const ART::Leaf* leaf) { suff_size += leaf->value().size_in_bytes(); });

        return ART::size_in_bytes(full_leaves) + suff_size;
    }

private:
    std::vector<std::unique_ptr<T>> m_data;
};

} // namespace suffix_trie

// NOLINTEND

#endif
