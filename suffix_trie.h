#include <memory>
#include <ranges>
#include <vector>

#include "art.h"

#ifndef SUFFIX_TRIE_H
#define SUFFIX_TRIE_H

// NOLINTBEGIN

namespace suffix_trie {

// Suffix leaf holds either value or a vector of pointers to other leaves. Node that single suffix
// leaf can be both a terminal node holding a value, and a suffix node which holds pointers to
// terminal leaves.
//
template<class T = void*>
class Suffix_leaf {
public:
    std::unique_ptr<T> m_value{nullptr};
    std::vector<typename art::ART<Suffix_leaf>::Leaf*> m_leaf_ptrs;
};

// Suffix trie. It is a key/value container which holds full key (string by default) which holds
// value (T) and all suffixes of a key which points to a full key. This provides the ability to
// search suffix strings instead of just exact key search. Combined with ART::prefix_search, this
// gives us powerfull search possibilities.
//
template<class T = void*>
class Suffix_trie : public art::ART<Suffix_leaf<T>> {
public:
    using Suffix_leaf = Suffix_leaf<T>;
    using ART = art::ART<Suffix_leaf>;
    using Leaf = ART::Leaf;
    using result = ART::result;

    // Inserts full suffix string (holding T value) and all of it's suffixes which will point to
    // full suffix.
    //
    template<class V = T>
    result insert_suffix(const std::string& suffix, V&& value = V{})
    {
        auto r = ART::insert(suffix, {}); // TODO: Implement emplace in ART and avoid this nonsence.

        auto& value_ptr = r->value().m_value;
        if (value_ptr) // Suffix already exist. Let caller decide what to do.
            return r;

        value_ptr = std::make_unique<T>(std::forward<V>(value));
        Leaf* leaf = r.get();

        uint8_t* begin = (uint8_t*)suffix.data();
        uint8_t* end = begin + suffix.size();

        while (++begin <= end) {
            // TODO: Implement emplace in ART and avoid this nonsence.
            auto res = ART::insert(begin, end - begin, {});
            auto& ptrs = res->value().m_leaf_ptrs;

            if (res) // Vector entry does not exist, so just push back new pointer.
                ptrs.push_back(leaf);
            else if (std::ranges::find(ptrs, leaf) == ptrs.end())
                ptrs.push_back(leaf);
        }

        return result{leaf, true};
    }

    // Erase full string with value and all of it's suffixes.
    //
    void erase_suffix(const std::string& suffix)
    {
        Leaf* main_leaf = ART::search(suffix);
        if (!main_leaf)
            return;

        auto& value_ptr = main_leaf->value().m_value;
        if (!value_ptr) // Check if there is a value in this leaf.
            return;

        uint8_t* suffix_start = (uint8_t*)suffix.data();
        uint8_t* end = suffix_start + suffix.size();
        uint8_t* start = end;

        while (start > suffix_start) {
            Leaf* leaf = ART::search(start, end - start);
            assert(leaf != nullptr);

            auto& ptrs = leaf->value().m_leaf_ptrs;
            assert(std::ranges::find(ptrs, main_leaf) != ptrs.end());

            std::erase(ptrs, main_leaf);
            if (ptrs.empty() && !leaf->value().m_value)
                ART::erase(start, end - start);

            --start;
        }

        value_ptr.reset();
        if (main_leaf->value().m_leaf_ptrs.empty())
            ART::erase(suffix);
    }

    // Returns a vector of T* where key matches str suffix.
    //
    [[nodiscard]] auto search_suffix(const std::string& str) const noexcept
    {
        std::vector<const T*> results;

        if (Leaf* leaf = ART::search(str)) {
            T* value = leaf->value().m_value.get();
            if (value)
                results.push_back(value);

            for (Leaf* leaf_ptr : leaf->value().m_leaf_ptrs) {
                value = leaf_ptr->value().m_value.get();
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
            T* value = leaf->value().m_value.get();
            if (value && std::ranges::find(results, value) == results.end())
                results.push_back(value);

            for (Leaf* leaf_ptr : leaf->value().m_leaf_ptrs) {
                value = leaf_ptr->value().m_value.get();
                assert(value);
                if (std::ranges::find(results, value) == results.end())
                    results.push_back(value);
            }
        }

        return results;
    }
};

} // namespace suffix_trie

// NOLINTEND

#endif
