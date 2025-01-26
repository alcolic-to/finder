#ifndef TRIE_H
#define TRIE_H

#pragma once

#include <algorithm>
#include <list>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

// NOLINTBEGIN

class Trie_node;

class Trie_node_edge {
public:
    Trie_node* m_node;
    // std::string m_suffix;
    char m_ch;
};

// Suffix tree node.
//
class Trie_node {
public:
    std::vector<std::string> all_results()
    {
        std::vector<std::string> res;

        for (auto it = m_edges.begin(); it != m_edges.end(); ++it) {
            std::vector<std::string> partial{it->m_node->all_results()};
            res.insert(res.end(), partial.begin(), partial.end());
        }

        res.insert(res.end(), m_results.begin(), m_results.end());
        return res;
    }

    std::list<Trie_node_edge> m_edges;
    std::vector<std::string> m_results; // all strings that are in theory represented as $.
};

class Trie {
public:
    explicit Trie(const std::string& s) { insert(s); }

    void insert(const std::string& s)
    {
        m_strings.push_back(s);

        for (int i = 0; i < s.size(); ++i)
            insert(&m_root, s, s.c_str() + i);
    }

    void insert(Trie_node* node, const std::string& s, const char* const suffix)
    {
        const char ch = *suffix;
        if (ch == '\0')
            return node->m_results.push_back(s);

        auto& edges = node->m_edges;
        auto it = edges.begin();

        for (; it != edges.end() && it->m_ch <= ch; ++it)
            if (it->m_ch == ch)
                return insert(it->m_node, s, suffix + 1);

        auto new_node_it = edges.insert(it, {.m_node = new Trie_node, .m_ch = ch});
        insert(new_node_it->m_node, s, suffix + 1);
    }

    Trie_node* find(std::string s) { return find(&m_root, s, s.c_str()); }

    Trie_node* find(Trie_node* node, const std::string& s, const char* const suffix)
    {
        const char ch = *suffix;
        if (ch == '\0')
            return node;

        auto& edges = node->m_edges;
        for (auto it = edges.begin(); it != edges.end() && it->m_ch <= ch; ++it)
            if (it->m_ch == ch)
                return find(it->m_node, s, suffix + 1);

        return nullptr;
    }

public:
    Trie_node m_root;
    std::vector<std::string> m_strings;
};

// NOLINTEND

#endif