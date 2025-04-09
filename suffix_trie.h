#include <functional>
#include <list>
#include <ranges>
#include <vector>

#include "art.h"

// NOLINTBEGIN

using strref = std::reference_wrapper<std::string>;

class Suffix_trie : private art::ART<std::vector<strref>> {
public:
    static constexpr bool path_sep(char ch) { return ch == '\\' || ch == '/'; }

    void insert_path(const std::string& path)
    {
        const char* start = path.data();
        const char* end = start;
        const char* path_end = start + path.size();

        while (start < path_end) {
            while (end < path_end && !path_sep(*end))
                ++end;

            if (start < end)
                insert_suffix(std::string{start, end}, path);

            ++end;
            start = end;
        }
    }

    void insert_suffix(const std::string& suffix) { insert_suffix(suffix, suffix); }

    // Inserts string in a trie.
    // This is done by inserting all suffixes of a string, where each leaf will
    // point to the physical string(s) stored in results (m_$) list.
    //
    void insert_suffix(const std::string& suffix, std::string value)
    {
        const auto& it = std::ranges::find(m_$, value);
        std::string& s = it != m_$.end() ? *it : m_$.emplace_back(std::move(value));

        uint8_t* begin = (uint8_t*)suffix.data();
        uint8_t* end = begin + suffix.size();

        while (begin <= end) {
            auto res = insert(begin, end - begin, s); // Try to insert new vector with s.
            if (!res) // Vector entry already exist, so just push back new string.
                res->value().push_back(s);

            ++begin;
        }
    }

    // Erases single string from suffix trie.
    //
    void erase_suffix(const std::string& str)
    {
        // Some code.
        //
        m_root.reset();
    }

    auto find_suffix(const std::string& str) { return search(str); }

    [[nodiscard]] auto find_prefix(const std::string& str) { return search_prefix(str); }

    const std::list<std::string>& $() const noexcept { return m_$; }

private:
    std::list<std::string> m_$;
};

// NOLINTEND
