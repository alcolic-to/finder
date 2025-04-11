#include <filesystem>
#include <map>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "art.h"

// NOLINTBEGIN

namespace suffix_trie {

// Suffix map holds full (suffix) strings with their related values. Since single suffix can have
// multiple values (for example, same string can be contained in multiple file paths), we will store
// vector of all values related to key string.
//
template<class T>
using suffix_map = std::map<std::string, T>;

// Pointer to single entry in a suffix map.
//
template<class T>
using suffix_map_it = suffix_map<T>::iterator;

// Vector of pointers to different prefixes in a map.
// One example for clarity:
// Let's say we want to map suffix string to normal string (default behaviour) and assume that we
// have 2 string entries: banana and ana. We will have 2 strings in suffix_map: banana and
// ana, and in ART we will have all suffixes of both strings: "banana", "anana", "nana", "ana",
// "na", "a", "". In art leaves "ana\0", "na\0", "a\0", "\0", we need to separate pointers to
// strings banana and ana, hence we will store pointers for those 2 map entries in vector of
// pointers in leaves.
//
template<class T>
using art_value_type = std::vector<suffix_map_it<T>>;

template<class T = void*>
class Suffix_trie : private art::ART<art_value_type<T>> {
public:
    using ART = art::ART<art_value_type<T>>;

    // Class that wraps insert result.
    // It holds iterator and a bool flag representing whether insert succeeded.
    //
    class result {
    public:
        using Iter = suffix_map_it<T>;

        result(Iter iterator, bool ok) : m_iterator{iterator}, m_ok{ok} {}

        Iter get() noexcept { return m_iterator; }

        const Iter get() const noexcept { return m_iterator; }

        constexpr bool ok() const noexcept { return m_ok; }

        Iter* operator->() noexcept { return get(); }

        const Iter* operator->() const noexcept { return get(); }

        constexpr operator bool() const noexcept { return ok(); }

    private:
        Iter m_iterator;
        bool m_ok;
    };

    static constexpr bool path_sep(char ch) { return ch == '\\' || ch == '/'; }

    // Inserts string in a trie.
    // This is done by inserting all suffixes of a string, where each leaf will
    // point to the physical string stored in results (m_$) list.
    //
    template<class V = T>
    result insert_suffix(std::string suffix, V&& value = V{})
    {
        if (auto r = m_$.find(suffix); r != m_$.end())
            return {r, false};

        auto entry = m_$.emplace(std::move(suffix), std::forward<V>(value));

        uint8_t* begin = (uint8_t*)entry.first->first.data();
        uint8_t* end = begin + entry.first->first.size();

        while (begin <= end) {
            auto res = ART::insert(begin, end - begin, entry.first);
            if (!res) // Vector entry already exist, so just push back new iterator.
                res->value().push_back(entry.first);

            ++begin;
        }

        return {entry.first, true};
    }

    // Erases single string from suffix trie.
    //
    void erase_suffix(const std::string& suffix)
    {
        auto entry = m_$.find(suffix);
        if (entry == m_$.end())
            return;

        uint8_t* suffix_start = (uint8_t*)suffix.data();
        uint8_t* end = suffix_start + suffix.size();
        uint8_t* start = end;

        while (start >= suffix_start) {
            auto leaf = ART::search(start, end - start);
            assert(leaf != nullptr);

            auto& vec = leaf->value();
            assert(std::ranges::find(vec, entry) != vec.end());

            std::erase(vec, entry);
            if (vec.empty())
                ART::erase(start, end - start);

            --start;
        }

        m_$.erase(entry);
    }

    // Returns a vector of string/value matching str prefix.
    //
    [[nodiscard]] auto search_suffix(const std::string& str) const noexcept
    {
        std::vector<suffix_map_it<T>> results;

        if (auto* leaf = ART::search(str)) {
            for (const auto& map_entry_it : leaf->value())
                if (std::ranges::find(results, map_entry_it) == results.end())
                    results.push_back(map_entry_it);
        }

        return results;
    }

    // Returns a vector of string/value matching str prefix.
    //
    [[nodiscard]] auto search_prefix(const std::string& str) const noexcept
    {
        std::vector<suffix_map_it<T>> results;

        const auto& leaves = ART::search_prefix(str);
        for (const auto& leaf : leaves) {
            for (const auto& map_entry_it : leaf->value())
                if (std::ranges::find(results, map_entry_it) == results.end())
                    results.push_back(map_entry_it);
        }

        return results;
    }

    const auto& $() const noexcept { return m_$; }

    auto& $() noexcept { return m_$; }

private:
    suffix_map<T> m_$;
};

} // namespace suffix_trie

// NOLINTEND
