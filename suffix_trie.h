#ifndef SUFFIX_TRIE_H
#define SUFFIX_TRIE_H

#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <ranges>
#include <vector>

#include "art.h"
#include "ptr_tag.h"

// NOLINTBEGIN

namespace suffix_trie {

// template<class T>
// class SLeaf_value {
// std::unique_ptr<T> m_value;
// };

template<class T>
class SLeaf;

template<class T>
class Full_leaf {
public:
    T* m_value{nullptr};
    std::vector<SLeaf<T>*> m_links;
};

// Wrapper class that holds either pointer to node or pointer to leaf.
// It is not a smart pointer, hence user must manage memory manually.
// In order to simplify assigning another pointer with freeing current resources, reset() funtion is
// implemented.
//
template<class T>
class SLeaf {
    using Full_leaf = Full_leaf<T>;

private:
    static constexpr uintptr_t value_tag = 0;
    static constexpr uintptr_t link_tag = 1;
    static constexpr uintptr_t full_leaf_tag = 1;
    static_assert(full_leaf_tag <= tag_bits);

public:
    constexpr SLeaf() noexcept : m_ptr{nullptr} {}

    constexpr SLeaf(nullptr_t) noexcept : m_ptr{nullptr} {}

    constexpr SLeaf(T* value) noexcept : m_ptr{value} {}

    constexpr SLeaf(const T* value) noexcept : m_ptr{value} {}

    constexpr SLeaf(SLeaf* leaf) noexcept : m_ptr{set_tag(leaf, link_tag)} {}

    constexpr SLeaf(const SLeaf* leaf) noexcept : m_ptr{set_tag(leaf, link_tag)} {}

    constexpr SLeaf(Full_leaf* leaf) noexcept : m_ptr{set_tag(leaf, full_leaf_tag)} {}

    constexpr SLeaf(const Full_leaf* leaf) noexcept : m_ptr{set_tag(leaf, full_leaf_tag)} {}

    [[nodiscard]] constexpr bool value() const noexcept { return tag(m_ptr) == value_tag; }

    [[nodiscard]] constexpr bool link() const noexcept { return tag(m_ptr) == link_tag; }

    [[nodiscard]] constexpr bool full_leaf() const noexcept { return tag(m_ptr) == full_leaf_tag; }

    [[nodiscard]] constexpr T* value_ptr() const noexcept
    {
        assert(value());
        return static_cast<T*>(m_ptr);
    }

    [[nodiscard]] constexpr SLeaf* link_ptr() const noexcept
    {
        assert(link());
        return static_cast<SLeaf*>(clear_tag(m_ptr));
    }

    [[nodiscard]] constexpr Full_leaf* full_leaf_ptr() const noexcept
    {
        assert(full_leaf());
        return static_cast<Full_leaf*>(clear_tag(m_ptr));
    }

    constexpr operator bool() const noexcept { return m_ptr != nullptr; }

    constexpr bool empty() const noexcept { return operator bool(); }

    constexpr bool has_value() const noexcept
    {
        return value() || (full_leaf() && full_leaf_ptr()->m_value);
    }

    void reset() noexcept
    {
        if (value())
            delete value_ptr();

        if (full_leaf()) {
            Full_leaf* full_leaf = full_leaf_ptr();
            if (full_leaf->m_value != nullptr)
                delete full_leaf->m_value;

            delete full_leaf;
        }

        m_ptr = nullptr;
    }

    void reset_value() noexcept
    {
        if (empty() || link())
            return;

        if (value())
            return reset();

        Full_leaf* full_leaf = full_leaf_ptr();
        if (full_leaf->m_value != nullptr)
            delete full_leaf->m_value;

        auto& links = full_leaf->m_links;
        assert(links.size() >= 1);

        if (links.size() == 1)
            *this = SLeaf{links[0]};
    }

    void insert_link(SLeaf* leaf) noexcept
    {
        if (empty())
            *this = SLeaf{leaf};
        else if (value())
            *this = SLeaf{new Full_leaf{value_ptr(), std::vector{leaf}}};
        else if (link())
            *this = SLeaf{new Full_leaf{{}, std::vector{link_ptr(), leaf}}};
        else {
            auto& links = full_leaf_ptr()->m_links;
            if (std::ranges::find(links, leaf) == links.end())
                links.push_back(leaf);
        }
    }

    void erase_link(SLeaf* leaf) noexcept
    {
        if (empty() || value())
            return;

        if (link()) {
            if (link_ptr() == leaf)
                reset();

            return;
        }

        Full_leaf* full_leaf = full_leaf_ptr();
        auto& links = full_leaf->m_links;
        if (std::ranges::find(links, leaf) == links.end())
            return;

        std::erase(links, leaf);

        if (links.size() == 1) {
            if (!full_leaf->m_value)
                *this = SLeaf{links[0]};

            return;
        }

        if (links.empty()) {
            if (full_leaf->m_value)
                *this = SLeaf{full_leaf->m_value};
            else
                reset();
        }
    }

    // Frees memory based on pointer type and sets pointer to other (nullptr by default).
    //
    // constexpr void reset(SLeaf other = nullptr) noexcept
    // {
    //     if (*this) {
    //         if (node()) {
    //             Node* n = node_ptr();
    //             switch (n->m_type) { // clang-format off
    //             case Node4_t:   delete n->node4();   break;
    //             case Node16_t:  delete n->node16();  break;
    //             case Node48_t:  delete n->node48();  break;
    //             case Node256_t: delete n->node256(); break;
    //             default: assert(!"Invalid case.");   break;
    //             } // clang-format on
    //         }
    //         else {
    //             delete leaf_ptr();
    //         }
    //     }

    //     *this = other;
    // }

private:
    void* m_ptr;
};

// Suffix leaf holds either value or a vector of pointers to other leaves. Node that single suffix
// leaf can be both a terminal node holding a value, and a suffix node which holds pointers to
// terminal leaves.
//
// template<class T = void*>
// class Suffix_leaf {
// public:
// const T* value() const noexcept { return m_value.get(); }

// T* value() noexcept { return m_value.get(); }

// auto& leaves() noexcept { return m_leaf_ptrs; }

// const auto& leaves() const noexcept { return m_leaf_ptrs; }

// auto& ptr() noexcept { return m_value; }

// const auto& ptr() const noexcept { return m_value; }

// // Only include parts not included in ART size_in_bytes.
// //
// const size_t size_in_bytes() const noexcept
// {
// return (m_value ? sizeof(T) : 0) +
// m_leaf_ptrs.size() * sizeof(typename art::ART<Suffix_leaf>::Leaf*);
// }

// private:
// std::unique_ptr<T> m_value{nullptr};
// std::vector<typename art::ART<Suffix_leaf>::Leaf*> m_leaf_ptrs;
// };

// Suffix trie. It is a key/value container which holds full key (string by default) which holds
// value (T) and all suffixes of a key which points to a full key. This provides the ability to
// search suffix strings instead of just exact key search. Combined with ART::prefix_search, this
// gives us powerfull search possibilities.
//
template<class T = void*>
class Suffix_trie : public art::ART<SLeaf<T>> {
public:
    using SLeaf = SLeaf<T>;
    using ART = art::ART<SLeaf>;
    using ART_leaf = ART::Leaf;
    using Node = ART::Node;
    using Node4 = ART::Node4;
    using Node16 = ART::Node16;
    using Node48 = ART::Node48;
    using Node256 = ART::Node256;

    // Class that wraps insert result.
    // It holds a pointer to Leaf and a bool flag representing whether insert succeeded (read insert
    // for more details).
    //
    class result {
    public:
        result(SLeaf* leaf, bool ok) : m_value{leaf}, m_ok{ok} { assert(m_value != nullptr); }

        SLeaf* get() noexcept { return m_value; }

        const SLeaf* get() const noexcept { return m_value; }

        constexpr bool ok() const noexcept { return m_ok; }

        SLeaf* operator->() noexcept { return get(); }

        const SLeaf* operator->() const noexcept { return get(); }

        constexpr operator bool() const noexcept { return ok(); }

    private:
        SLeaf* m_value;
        bool m_ok;
    };

    // Inserts full suffix string (holding T value) and all of it's suffixes which will point to
    // full suffix.
    //
    template<class V = T>
    result insert_suffix(const std::string& suffix, V&& value = V{})
    {
        auto r = ART::insert(suffix, {}); // TODO: Implement emplace in ART and avoid this nonsence.

        auto& sleaf = r->value();
        if (sleaf) // Suffix already exist. Let caller decide what to do.
            return {&r->value(), false};

        sleaf = new T{std::forward<V>(value)}; // TODO: Check if this works.

        uint8_t* begin = (uint8_t*)suffix.data();
        uint8_t* end = begin + suffix.size();

        while (++begin <= end) {
            // TODO: Implement emplace in ART and avoid this nonsence.
            auto res = ART::insert(begin, end - begin, {});
            res->value().insert_link(&sleaf);

            // Old code:
            // auto& leaves = res->value().leaves();

            // if (res) // Vector entry does not exist, so just push back new pointer.
            // leaves.push_back(sleaf);
            // else if (std::ranges::find(leaves, sleaf) == leaves.end())
            // leaves.push_back(sleaf);
        }

        return result{&r->value(), true};
    }

    // Erase full string with value and all of it's suffixes.
    //
    void erase_suffix(const std::string& suffix)
    {
        ART_leaf* main_leaf = ART::search(suffix);
        if (!main_leaf)
            return;

        auto& main_sleaf = main_leaf->value();
        if (!main_sleaf.has_value()) // Check if there is a value in this sleaf.
            return;

        uint8_t* suffix_start = (uint8_t*)suffix.data();
        uint8_t* end = suffix_start + suffix.size();
        uint8_t* start = end;

        while (start > suffix_start) {
            ART_leaf* leaf = ART::search(start, end - start);
            assert(leaf != nullptr);

            auto& sleaf = leaf->value();

            sleaf.erase_link(&main_sleaf);
            if (sleaf.empty())
                ART::erase(start, end - start);

            --start;
        }

        main_sleaf.reset_value();
        if (main_sleaf.empty())
            ART::erase(suffix);
    }

    // Returns a vector of T* where key matches str suffix.
    //
    [[nodiscard]] auto search_suffix(const std::string& str) const noexcept
    {
        std::vector<const T*> results;

        if (ART_leaf* leaf = ART::search(str)) {
            T* value = leaf->value().ptr().get();
            if (value)
                results.push_back(value);

            for (ART_leaf* leaf_ptr : leaf->value().leaves()) {
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

            for (ART_leaf* leaf_ptr : leaf->value().leaves()) {
                value = leaf_ptr->value().ptr().get();
                assert(value);
                if (std::ranges::find(results, value) == results.end())
                    results.push_back(value);
            }
        }

        return results;
    }

    void print_stats()
    {
        constexpr size_t max_links = 16;
        size_t dist[2][max_links];
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < max_links; ++j)
                dist[i][j] = 0;

        size_t high = 0;
        size_t key_sizes = 0;
        size_t it_count = 0;
        size_t with_value = 0;
        size_t without_value = 0;
        size_t total_links_count = 0;

        ART::for_each_leaf([&](const ART::Leaf* leaf) {
            const SLeaf& sl = leaf->value();

            bool has_value = !!sl.ptr();
            with_value += has_value;
            without_value += !has_value;

            auto& leaves = sl.leaves();

            size_t links_count = std::min(leaves.size(), max_links - 1);
            total_links_count += leaves.size();
            high = std::max(high, leaves.size());

            // if (leaves.size() >= max_links)
            //     std::cout << "Links number too big: " << leaves.size()
            //               << ". Value: " << leaf->key_to_string() << "\n";

            ++dist[has_value][links_count];
            ++it_count;
            key_sizes += leaf->key_size();
        });

        std::cout << "---------------------------------------\n";
        std::cout << "Trie size in bytes:   " << size_in_bytes() << "\n";
        std::cout << "Nodes size in bytes:  " << ART::nodes_size_in_bytes() << "\n";
        std::cout << "Leaves size in bytes: " << leaves_size_in_bytes() << "\n";
        std::cout << "Nodes count:          " << ART::nodes_count() << "\n";
        std::cout << "Leaves count:         " << ART::leaves_count() << "\n";
        std::cout << "Total key sizes:      " << key_sizes << "\n";
        std::cout << "Avg key size:         " << key_sizes / it_count << "\n";
        std::cout << "Leaves with value:    " << with_value << "\n";
        std::cout << "Leaves without value: " << without_value << "\n";
        std::cout << "Total links count:    " << total_links_count << "\n";
        std::cout << "Max links per node:   " << high << "\n";

        std::cout << "Links count:";
        for (int i = 0; i < max_links - 1; ++i)
            std::cout << std::format(" {:>7}", i);

        std::cout << std::format(" >={:>5}", max_links);

        std::cout << "\nValue 0:    ";
        for (int i = 0; i < max_links; ++i)
            std::cout << std::format(" {:>7}", dist[false][i]);

        std::cout << "\nValue 1:    ";
        for (int i = 0; i < 16; ++i)
            std::cout << std::format(" {:>7}", dist[true][i]);

        std::cout << "\n---------------------------------------\n";
    }

    size_t leaves_size_in_bytes()
    {
        size_t size = 0;
        ART::for_each_leaf([&](const ART::Leaf* leaf) {
            size += sizeof(*leaf) + leaf->key_size() + leaf->value().size_in_bytes();
        });

        return size;
    }

    size_t size_in_bytes() { return ART::nodes_size_in_bytes() + leaves_size_in_bytes(); }
};

} // namespace suffix_trie

// NOLINTEND

#endif
