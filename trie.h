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
    std::list<Trie_node_edge> m_edges;
    std::vector<std::string> m_results; // all strings that are in theory represented as $.
};

class Trie {
public:
    explicit Trie(const std::string& str)
    {
        m_strings.push_back(str);

        for (int i = 0; i < str.size(); ++i)
            insert(&m_root, str, str.c_str() + i);
    }

    void insert(Trie_node* node, const std::string& str, const char* const suffix)
    {
        const char ch = *suffix;
        if (ch == '\0')
            return node->m_results.push_back(str);

        auto& edges = node->m_edges;
        auto it = edges.begin();

        for (; it != edges.end() && it->m_ch <= ch; ++it)
            if (it->m_ch == ch)
                return insert(it->m_node, str, suffix + 1);

        auto new_node_it = edges.insert(it, {.m_node = new Trie_node, .m_ch = ch});
        insert(new_node_it->m_node, str, suffix + 1);
    }

public:
    Trie_node m_root;
    std::vector<std::string> m_strings;
};

// NOLINTEND

#endif