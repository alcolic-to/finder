#ifndef SUFFIX_TRIE_H
#define SUFFIX_TRIE_H

#include <algorithm>
#include <cstdint>
#include <format>
#include <iostream>
#include <limits>
#include <vector>

#include "art.h"
#include "ptr_tag.h"

// NOLINTBEGIN

namespace suffix_trie {

template<class T>
class SLeaf;

// Full leaf which hold pointer to value and a vector of links to leaf nodes.
//
template<class T>
class Full_leaf {
public:
    T* m_value{nullptr};
    std::vector<SLeaf<T>*> m_links;
};

// Wrapper class that holds either pointer to value, pointer to full leaf node or pointer to another
// sleaf. It is not a smart pointer, hence user must manage memory manually. In order to simplify
// assigning another pointer with freeing current resources, reset() funtion is implemented.
//
template<class T>
class SLeaf {
    using Full_leaf = Full_leaf<T>;

private:
    static constexpr uintptr_t value_tag = 0;
    static constexpr uintptr_t link_tag = 1;
    static constexpr uintptr_t full_leaf_tag = 2;
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

    constexpr bool empty() const noexcept { return !operator bool(); }

    T* get_value()
    {
        if (empty() || link())
            return nullptr;

        if (value())
            return value_ptr();

        return full_leaf_ptr()->m_value;
    }

    constexpr bool has_value() const noexcept
    {
        return !empty() && (value() || (full_leaf() && full_leaf_ptr()->m_value != nullptr));
    }

    void insert_value(T* value) noexcept
    {
        assert(!has_value());

        if (empty())
            *this = value;
        else if (link())
            *this = new Full_leaf{value, std::vector{link_ptr()}};
        else {
            assert(full_leaf());
            full_leaf_ptr()->m_value = value;
        }
    }

    // Deletes value and shrinks leaf if possible.
    //
    void delete_value() noexcept
    {
        if (empty() || link())
            return;

        if (value())
            return reset();

        Full_leaf* full_leaf = full_leaf_ptr();
        if (full_leaf->m_value != nullptr) {
            delete full_leaf->m_value;
            full_leaf->m_value = nullptr;
        }

        auto& links = full_leaf->m_links;
        assert(links.size() >= 1);

        if (links.size() == 1) {
            *this = links[0];
            delete full_leaf;
        }
    }

    void insert_link(SLeaf* leaf) noexcept
    {
        if (empty())
            *this = leaf;
        else if (value())
            *this = new Full_leaf{value_ptr(), std::vector{leaf}};
        else if (link())
            *this = new Full_leaf{nullptr, std::vector{link_ptr(), leaf}};
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
            if (!full_leaf->m_value) {
                *this = SLeaf{links[0]};
                delete full_leaf;
            }

            return;
        }

        if (links.empty()) {
            assert(full_leaf->m_value != nullptr);
            *this = SLeaf{full_leaf->m_value};
            delete full_leaf;
        }
    }

    void reset() noexcept
    {
        if (empty())
            return;

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

    size_t size_in_bytes() const noexcept
    {
        size_t size = sizeof(*this);

        if (value())
            size += sizeof(T);
        else if (full_leaf()) {
            Full_leaf* full_leaf = full_leaf_ptr();

            size += sizeof(Full_leaf);

            if (full_leaf->m_value)
                size += sizeof(T);

            size += full_leaf->m_links.size() * sizeof(SLeaf*);
        }

        return size;
    }

private:
    void* m_ptr;
};

// Suffix trie. It is a key/value container which holds full key (string by default) which holds
// value (T) and all suffixes of a key which points to a full key. This provides the ability to
// search suffix strings instead of just exact key search. Combined with ART::prefix_search, this
// gives us powerfull search possibilities.
//
template<class T = void*>
class Suffix_trie : public art::ART<SLeaf<T>> {
public:
    using SLeaf = SLeaf<T>;
    using Full_leaf = Full_leaf<T>;
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

    // I am too lazy to implement proper destruction. In order to do that, destructor of SLeaf
    // should be implemented, and it will be invoked automatically when ART destroys it's nodes.
    // That would save whole tree traversal, but who cares...
    //
    ~Suffix_trie()
    {
        ART::for_each_leaf([](ART::Leaf* leaf) { leaf->value().reset(); });
    }

    // Inserts full suffix string (holding T value) and all of it's suffixes which will point to
    // full suffix.
    //
    template<class V = T>
    result insert_suffix(const std::string& suffix, V&& value = V{})
    {
        auto r = ART::insert(suffix, {}); // TODO: Implement emplace in ART and avoid this nonsence.

        auto& sleaf = r->value();
        if (sleaf.get_value() != nullptr) // Suffix already exist. Let caller decide what to do.
            return {&sleaf, false};

        sleaf.insert_value(new T{std::forward<V>(value)});

        uint8_t* begin = (uint8_t*)suffix.data();
        uint8_t* end = begin + suffix.size();

        while (++begin <= end) {
            // TODO: Implement emplace in ART and avoid this nonsence.
            auto res = ART::insert(begin, end - begin, {});
            res->value().insert_link(&sleaf);
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

        main_sleaf.delete_value();
        if (main_sleaf.empty())
            ART::erase(suffix);
    }

    // Returns a vector of T* where key matches str suffix.
    //
    [[nodiscard]] auto search_suffix(const std::string& str) const noexcept
    {
        std::vector<const T*> results;

        auto insert_value = [&](SLeaf* sleaf) {
            T* v = sleaf->get_value();
            if (v != nullptr && std::ranges::find(results, v) == results.end())
                results.push_back(v);
        };

        if (ART_leaf* leaf = ART::search(str)) {
            SLeaf* sleaf = &leaf->value();
            insert_value(sleaf);

            if (sleaf->link())
                insert_value(sleaf->link_ptr());
            else if (sleaf->full_leaf())
                for (SLeaf* link : sleaf->full_leaf_ptr()->m_links)
                    insert_value(link);
        }

        return results;
    }

    // Returns a vector of T* where key matches str prefix.
    // Since prefix search can have a lot of entries, user can limit number of results.
    //
    [[nodiscard]] auto
    search_prefix(const std::string& str,
                  size_t limit = std::numeric_limits<size_t>::max()) const noexcept
    {
        std::vector<const T*> results;

        auto insert_value = [&](SLeaf* sleaf) {
            T* v = sleaf->get_value();
            if (v != nullptr && std::ranges::find(results, v) == results.end())
                results.push_back(v);

            return results.size() < limit;
        };

        const auto& leaves = ART::search_prefix(str, limit);
        for (auto& leaf : leaves) {
            SLeaf* sleaf = &leaf->value();
            if (!insert_value(sleaf))
                break;

            if (sleaf->link()) {
                if (!insert_value(sleaf->link_ptr()))
                    break;
            }
            else if (sleaf->full_leaf()) {
                for (SLeaf* link : sleaf->full_leaf_ptr()->m_links)
                    if (!insert_value(link))
                        break;
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

        ART::for_each_leaf([&](ART::Leaf* leaf) {
            ++it_count;

            SLeaf& sl = leaf->value();

            bool has_value = sl.get_value() != nullptr;
            with_value += has_value;
            without_value += !has_value;

            size_t links_count = 0;

            if (sl.link()) {
                links_count = 1;
                total_links_count += 1;
                high = std::max(high, 1ULL);
            }
            else if (sl.full_leaf()) {
                auto& leaves = sl.full_leaf_ptr()->m_links;
                links_count = std::min(leaves.size(), max_links - 1);
                total_links_count += leaves.size();
                high = std::max(high, leaves.size());

                // if (leaves.size() >= max_links)
                //     std::cout << "Links number too big: " << leaves.size()
                //               << ". Value: " << leaf->key_to_string() << "\n";
            }

            ++dist[has_value][links_count];
            key_sizes += leaf->key_size();
        });

        std::cout << "-------------------------------\n";
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

        std::cout << "\n-------------------------------\n";
    }

    size_t leaves_size_in_bytes()
    {
        size_t size = 0;
        ART::for_each_leaf([&](const ART::Leaf* leaf) {
            size += sizeof(*leaf) + leaf->key_size() + leaf->value().size_in_bytes() -
                    sizeof(leaf->value());
        });

        return size;
    }

    size_t size_in_bytes() { return ART::nodes_size_in_bytes() + leaves_size_in_bytes(); }
};

} // namespace suffix_trie

// NOLINTEND

#endif
