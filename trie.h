#ifndef TRIE_H
#define TRIE_H

#pragma once

#include <algorithm>
#include <cstddef>
#include <list>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>

// NOLINTBEGIN

void rm_common_prefix(std::string_view& sv1, std::string_view& sv2)
{
    const size_t min_size = std::min(sv1.size(), sv2.size());
    size_t idx = 0;
    while (idx < min_size && sv1[idx] == sv2[idx])
        ++idx;

    sv1.remove_prefix(idx), sv2.remove_prefix(idx);
}

class Trie_node;

class Trie_node_edge {
public:
    Trie_node_edge() = default;

    Trie_node_edge(Trie_node* node, std::string_view suffix)
        : m_node{node}
        , m_suffix{std::move(suffix)}
    {
    }

    Trie_node* m_node;
    std::string m_suffix;

    char ch() { return m_suffix[0]; }
};

// Suffix tree node.
//
class Trie_node {
public:
    Trie_node() = default;

    Trie_node(Trie_node_edge edge) : m_edges{edge} {}

    Trie_node(std::string_view s) : m_${std::move(s)} {}

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
};

class Trie {
public:
    Trie() = default;

    explicit Trie(const std::string& s) { insert(s); }

    void insert(std::string new_string)
    {
        if (std::ranges::find(m_$s, new_string) != m_$s.end())
            return;

        m_$s.emplace_back(std::move(new_string));
        auto& str{m_$s.back()};

        for (auto it{str.begin()}; it <= str.end(); ++it) {
            insert(&m_root, std::string_view{it, str.end()}, str);

            if (it == str.end()) // Avoid assertion in debug build for ++it if it >= end().
                break;
        }
    }

    void insert(Trie_node_edge* edge, std::string_view suffix, const std::string& full_str)
    {
        std::string_view edge_suffix{edge->m_suffix};
        rm_common_prefix(suffix, edge_suffix);

        if (!edge_suffix.empty()) {
            edge->m_node = new Trie_node{Trie_node_edge{edge->m_node, edge_suffix}};
            edge->m_suffix = edge->m_suffix.substr(0, edge->m_suffix.size() - edge_suffix.size());
        }

        insert(edge->m_node, suffix, full_str);
    }

    void insert(Trie_node* node, std::string_view suffix, const std::string& full_str)
    {
        if (suffix.empty())
            return node->m_$.push_back(full_str);

        auto& edges = node->m_edges;
        auto it{edges.begin()};

        for (; it != edges.end() && it->ch() <= suffix[0]; ++it)
            if (it->ch() == suffix[0])
                return insert(&*it, suffix, full_str);

        edges.insert(it, Trie_node_edge{new Trie_node{full_str}, suffix});
    }

    void delete_suffix(std::string str)
    {
        auto $s_iter{std::ranges::find(m_$s, str)};
        if ($s_iter == m_$s.end())
            return;

        for (auto it{str.end()}; it >= str.begin(); --it) {
            delete_suffix(&m_root, std::string_view{it, str.end()}, str);

            if (it == str.begin()) // Avoid assertion in debug build for --it if it <= begin().
                break;
        }

        m_$s.erase($s_iter);
    }

    void delete_suffix(Trie_node_edge* edge, std::string_view suffix, const std::string& full_str)
    {
        std::string_view edge_suffix{edge->m_suffix};
        rm_common_prefix(suffix, edge_suffix);

        if (edge_suffix.empty()) {
            delete_suffix(edge->m_node, suffix, full_str);

            if (edge->m_node->m_$.empty()) {
                Trie_node* del_node = edge->m_node;
                // assert(del_node->m_edges.size() <= 1);

                if (del_node->m_edges.size() == 1) {
                    edge->m_suffix += del_node->m_edges.front().m_suffix;
                    edge->m_node = del_node->m_edges.front().m_node;
                    delete del_node;
                }
            }
        }
    }

    void delete_suffix(Trie_node* node, std::string_view suffix, const std::string& full_str)
    {
        if (suffix.empty()) {
            node->m_$.remove(full_str);
            return;
        }

        auto& edges = node->m_edges;
        auto it{edges.begin()};

        for (; it != edges.end() && it->ch() <= suffix[0]; ++it) {
            if (it->ch() == suffix[0]) {
                delete_suffix(&*it, suffix, full_str);

                if (it->m_node->m_$.empty() && it->m_node->m_edges.empty()) {
                    delete it->m_node;
                    edges.erase(it);
                }

                return;
            }
        }
    }

    Trie_node* find(std::string s) { return find(&m_root, s); }

    Trie_node* find(Trie_node_edge* edge, std::string_view suffix)
    {
        std::string_view edge_suffix{edge->m_suffix};
        rm_common_prefix(suffix, edge_suffix);

        return !suffix.empty() && !edge_suffix.empty() ? nullptr : find(edge->m_node, suffix);
    }

    Trie_node* find(Trie_node* node, std::string_view suffix)
    {
        if (suffix.empty())
            return node;

        auto& edges = node->m_edges;
        for (auto it{edges.begin()}; it != edges.end() && it->ch() <= suffix[0]; ++it)
            if (it->ch() == suffix[0])
                return find(&*it, suffix);

        return nullptr;
    }

public:
    Trie_node m_root;
    std::list<std::string> m_$s; // Physical strings that must not move.
};

// NOLINTEND

#endif
