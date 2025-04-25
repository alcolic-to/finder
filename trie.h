#ifndef TRIE_H
#define TRIE_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>

// NOLINTBEGIN

// Options for lexicographical comparison.
//
// normal -> compares characters as they are.
// icase -> compares characters ignoring case.
//
enum class Options { normal, icase };

// Returns result of 2 characters comparisson.
//
template<Options opt = Options::normal>
inline bool chcmp(const char ch1, const char ch2)
{
    if constexpr (opt == Options::normal)
        return ch1 == ch2;
    else if constexpr (opt == Options::icase)
        return std::tolower(ch1) == std::tolower(ch2);
}

// Removes common previx of 2 string views.
//
template<Options opt = Options::normal>
void rm_common_prefix(std::string_view& sv1, std::string_view& sv2)
{
    const size_t min_size = std::min(sv1.size(), sv2.size());
    size_t idx = 0;
    while (idx < min_size && chcmp<opt>(sv1[idx], sv2[idx]))
        ++idx;

    sv1.remove_prefix(idx), sv2.remove_prefix(idx);
}

class Trie_node;

// Trie node edge that holds pointer to node and string suffix.
//
class Trie_node_edge {
public:
    Trie_node_edge() = default;

    Trie_node_edge(std::unique_ptr<Trie_node> node, std::string_view suffix)
        : m_node{std::move(node)}
        , m_suffix{std::move(suffix)}
    {
    }

    std::unique_ptr<Trie_node> m_node;
    std::string m_suffix;

    char ch() { return m_suffix[0]; }
};

// Suffix tree node that holds list of edges and list of results ($).
// All edges are sorted by the first letter in edge.
//
class Trie_node {
public:
    Trie_node() = default;

    Trie_node(Trie_node_edge edge, Trie_node* prev) : m_prev{prev}
    {
        m_edges.emplace_back(std::move(edge));
    }

    Trie_node(std::string_view s, Trie_node* prev) : m_${std::move(s)}, m_prev{prev} {}

    std::unordered_set<std::string_view> all_results()
    {
        std::unordered_set<std::string_view> results;
        all_results_internal(results);

        return results;
    }

    void all_results_internal(std::unordered_set<std::string_view>& results)
    {
        results.insert(m_$.begin(), m_$.end());

        for (auto& it : m_edges)
            it.m_node->all_results_internal(results);
    }

    std::list<Trie_node_edge> m_edges;
    std::list<std::string_view> m_$; // all result strings that are in theory represented as $.
    Trie_node* m_prev = nullptr;     // pointer to the previous node.
};

// Trie. It is a representation of a compresses suffix tree ->
// https://en.wikipedia.org/wiki/Suffix_tree.
//
// Holds root node pointer and a list of a physical strings that are inserted in trie.
// Strings must not move, because nodes holds string_views to physical strings, hence we use
// std::list (iterators are never invaliated).
//
class Trie {
public:
    Trie() = default;

    explicit Trie(const std::string& s) { insert(s); }

    // Inserts string in a trie.
    // This is done by inserting all suffixes of a string, and each leaf node of all suffixes will
    // point to the physical string stored in results (m_$s) list.
    //
    void insert(std::string new_string)
    {
        if (std::ranges::find(m_$s, new_string) != m_$s.end())
            return;

        m_$s.emplace_back(std::move(new_string));
        auto& str{m_$s.back()};

        for (auto it{str.begin()}; it <= str.end(); ++it) {
            insert(m_root, std::string_view{it, str.end()}, str);

            if (it == str.end()) // Avoid assertion in debug build for ++it if it >= end().
                break;
        }
    }

    // Inserts single suffix in a trie.
    // This function just traveses the edge suffix string and creates a new node if whole suffix is
    // not traversed. Note that when new node is created, we must link the old node to the edge in
    // new node and split edge suffix on separation point from suffix beeing inserted.
    //
    void insert(Trie_node_edge& edge, std::string_view suffix, const std::string& full_str)
    {
        std::string_view edge_suffix{edge.m_suffix};
        rm_common_prefix(suffix, edge_suffix);

        if (!edge_suffix.empty()) {
            Trie_node* old_edge_node = edge.m_node.get();
            Trie_node* backptr = old_edge_node->m_prev;

            edge.m_node = std::make_unique<Trie_node>(
                Trie_node_edge{std::move(edge.m_node), edge_suffix}, backptr);
            edge.m_suffix = edge.m_suffix.substr(0, edge.m_suffix.size() - edge_suffix.size());

            old_edge_node->m_prev = edge.m_node.get();
        }

        insert(edge.m_node, suffix, full_str);
    }

    // Inserts single suffix in a trie.
    // It finds correct place where suffix needs to be inserted (all edges are sorted based on first
    // letter in edge) if it does not exist or calls another insert function that traverses the
    // edge if edge with that letter already exist.
    // If suffix is empty, it means that we are at the leaf node, so we insert string view to the
    // physical string.
    //
    void insert(const std::unique_ptr<Trie_node>& node, const std::string_view suffix,
                const std::string& full_str)
    {
        if (suffix.empty())
            return node->m_$.push_back(full_str);

        auto& edges = node->m_edges;
        auto it{edges.begin()};

        for (; it != edges.end() && it->ch() <= suffix[0]; ++it)
            if (it->ch() == suffix[0])
                return insert(*it, suffix, full_str);

        edges.insert(it, Trie_node_edge{std::make_unique<Trie_node>(full_str, node.get()), suffix});
    }

    // Deletes string (suffix) from a trie.
    // It uses the reverse logic of insertion: calls delete from empty string up to full string
    // by adding single letter at a time from the end of string. This will remove all suffixes of a
    // string from a trie.
    //
    void delete_suffix(std::string str)
    {
        auto $s_iter{std::ranges::find(m_$s, str)};
        if ($s_iter == m_$s.end())
            return;

        for (auto it{str.end()}; it >= str.begin(); --it) {
            delete_suffix(m_root, std::string_view{it, str.end()}, str);

            if (it == str.begin()) // Avoid assertion in debug build for --it if it <= begin().
                break;
        }

        m_$s.erase($s_iter);
    }

    // Similar logic as an insertion of a suffix.
    //
    void delete_suffix(Trie_node_edge& edge, std::string_view suffix, const std::string& full_str)
    {
        std::string_view edge_suffix{edge.m_suffix};
        rm_common_prefix(suffix, edge_suffix);

        if (edge_suffix.empty()) {
            delete_suffix(edge.m_node, suffix, full_str);

            // Merge edges if there are no results in between.
            //
            if (edge.m_node->m_$.empty() && edge.m_node->m_edges.size() == 1) {
                Trie_node* backptr = edge.m_node->m_prev;

                edge.m_suffix += edge.m_node->m_edges.front().m_suffix;
                edge.m_node = std::move(edge.m_node->m_edges.front().m_node);
                edge.m_node->m_prev = backptr;
            }
        }
    }

    // Similar logic as an insertion of a suffix. See insert() for more details.
    //
    void delete_suffix(const std::unique_ptr<Trie_node>& node, const std::string_view suffix,
                       const std::string& full_str)
    {
        if (suffix.empty()) {
            node->m_$.remove(full_str);
            return;
        }

        auto& edges = node->m_edges;
        auto it{edges.begin()};

        for (; it != edges.end() && it->ch() <= suffix[0]; ++it) {
            if (it->ch() == suffix[0]) {
                delete_suffix(*it, suffix, full_str);

                if (it->m_node->m_$.empty() && it->m_node->m_edges.empty())
                    edges.erase(it);

                return;
            }
        }
    }

    // Finds all strings that contains string s.
    //
    template<Options opt = Options::normal>
    std::unordered_set<std::string_view> find(std::string s)
    {
        std::unordered_set<std::string_view> results;

        for (auto it : find_nodes<opt>(std::move(s))) {
            auto res_from_node{it->all_results()};
            results.insert(res_from_node.begin(), res_from_node.end());
        }

        return results;
    }

    // Finds all nodes that corresponds to a string s.
    // Nodes are found by travesing all edges that have the same value as s.
    // Note that search can be case insesitive, hence we can have more than a single result.
    //
    template<Options opt>
    std::vector<Trie_node*> find_nodes(std::string s)
    {
        std::vector<Trie_node*> results;
        results.reserve(2);

        find_nodes<opt>(m_root, s, results);

        return results;
    }

    // Traverses the edge by removing common prefixes.
    //
    template<Options opt>
    void find_nodes(Trie_node_edge& edge, std::string_view suffix, std::vector<Trie_node*>& results)
    {
        std::string_view edge_suffix{edge.m_suffix};
        rm_common_prefix<opt>(suffix, edge_suffix);

        if (suffix.empty() || edge_suffix.empty())
            find_nodes<opt>(edge.m_node, suffix, results);
    }

    // Similar logic to suffix insertions. See insert() for more details.
    //
    template<Options opt>
    void find_nodes(const std::unique_ptr<Trie_node>& node, const std::string_view suffix,
                    std::vector<Trie_node*>& results)
    {
        if (suffix.empty()) {
            results.emplace_back(node.get());
            return;
        }

        auto& edges = node->m_edges;
        for (auto it{edges.begin()}; it != edges.end(); ++it)
            if (chcmp<opt>(it->ch(), suffix[0]))
                find_nodes<opt>(*it, suffix, results);
    }

public:
    std::unique_ptr<Trie_node> m_root = std::make_unique<Trie_node>();
    std::list<std::string> m_$s; // Physical strings that must not move.
};

// NOLINTEND

#endif
