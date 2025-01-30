#ifndef TRIE_H
#define TRIE_H

#pragma once

#include <algorithm>
#include <list>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

// NOLINTBEGIN

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
    std::string_view m_suffix;

    char ch() { return m_suffix[0]; }

    // char m_ch;
};

// Suffix tree node.
//
class Trie_node {
public:
    Trie_node() = default;

    Trie_node(Trie_node_edge edge) : m_edges{edge} {}

    Trie_node(std::string_view suffix) : m_results{suffix} {}

    std::vector<std::string_view> all_results()
    {
        std::vector<std::string_view> results;
        all_results_internal(results);

        return results;
    }

    void all_results_internal(std::vector<std::string_view>& results)
    {
        results.insert(results.end(), m_results.begin(), m_results.end());

        for (auto it = m_edges.begin(); it != m_edges.end(); ++it)
            it->m_node->all_results_internal(results);
    }

    std::list<Trie_node_edge> m_edges;
    std::list<std::string_view> m_results; // all strings that are in theory represented as $.
};

class Trie {
public:
    Trie() = default;

    explicit Trie(const std::string& s) { insert(s); }

    void insert(std::string new_suffix)
    {
        m_strings.emplace_back(std::move(new_suffix));
        auto& suffix{m_strings.back()};

        for (auto it{suffix.begin()}; it != suffix.end(); ++it)
            insert(&m_root, suffix, std::string_view{it, suffix.end()});
    }

    void insert(Trie_node_edge* edge, const std::string& s, std::string_view suffix)
    {
        std::string_view edge_suf{edge->m_suffix};
        while (!suffix.empty() && !edge_suf.empty() && suffix[0] == edge_suf[0])
            suffix.remove_prefix(1), edge_suf.remove_prefix(1);

        if (edge_suf.empty()) {
            if (suffix.empty())
                return edge->m_node->m_results.push_back(s);

            return insert(edge->m_node, s, suffix);
        }

        edge->m_node = new Trie_node{Trie_node_edge{edge->m_node, edge_suf}};
        edge->m_suffix = std::string_view{edge->m_suffix.begin(), edge_suf.begin()};

        insert(edge->m_node, s, suffix);
    }

    void insert(Trie_node* node, const std::string& s, std::string_view suffix)
    {
        if (suffix.empty())
            return node->m_results.push_back(s);

        auto& edges = node->m_edges;
        auto it = edges.begin();

        for (; it != edges.end() && it->ch() <= suffix[0]; ++it)
            if (it->ch() == suffix[0])
                return insert(&*it, s, suffix);

        edges.insert(it, {new Trie_node{s}, suffix});
    }

    Trie_node* find(std::string s) { return find(&m_root, s); }

    Trie_node* find(Trie_node_edge* edge, std::string_view suffix)
    {
        std::string_view edge_suf{edge->m_suffix};
        while (!suffix.empty() && !edge_suf.empty() && suffix[0] == edge_suf[0])
            suffix.remove_prefix(1), edge_suf.remove_prefix(1);

        if (suffix.empty())
            return edge->m_node;

        if (edge_suf.empty())
            return find(edge->m_node, suffix);

        return nullptr;
    }

    Trie_node* find(Trie_node* node, std::string_view suffix)
    {
        if (suffix.empty())
            return node;

        auto& edges = node->m_edges;
        for (auto it = edges.begin(); it != edges.end() && it->ch() <= suffix[0]; ++it)
            if (it->ch() == suffix[0])
                return find(&*it, suffix);

        return nullptr;
    }

public:
    Trie_node m_root;
    std::list<std::string> m_strings;
};

// NOLINTEND

#endif
