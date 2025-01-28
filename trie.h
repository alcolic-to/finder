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

    Trie_node_edge(Trie_node* node, std::string suffix) : m_node{node}, m_suffix{std::move(suffix)}
    {
    }

    Trie_node* m_node;
    std::string m_suffix;

    char ch() { return m_suffix[0]; }

    // char m_ch;
};

// Suffix tree node.
//
class Trie_node {
public:
    Trie_node() = default;

    Trie_node(Trie_node_edge edge) : m_edges{edge} {}

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
    Trie() = default;

    explicit Trie(const std::string& s) { insert(s); }

    void insert(const std::string& s)
    {
        m_strings.push_back(s);

        for (int i = 0; i < s.size(); ++i)
            insert(&m_root, s, (char*)(&s[0]) + i);
    }

    void insert(Trie_node_edge* edge, const std::string& s, char* suffix)
    {
        char* suf = suffix;
        char* ed_suf = &edge->m_suffix[0];

        while (*suf && *ed_suf && *suf == *ed_suf)
            ++suf, ++ed_suf;

        if (!*ed_suf) {
            if (!*suf) // We already have a full text in edge. Just insert result.
                return edge->m_node->m_results.push_back(s);

            return insert(edge->m_node, s, suf);
        }

        edge->m_node = new Trie_node{Trie_node_edge{edge->m_node, ed_suf}};
        edge->m_suffix = edge->m_suffix.substr(0, ed_suf - &edge->m_suffix[0]);

        insert(edge->m_node, s, suf);
    }

    void insert(Trie_node* node, const std::string& s, char* suffix)
    {
        const char ch = *suffix;
        if (ch == '\0')
            return node->m_results.push_back(s);

        auto& edges = node->m_edges;
        auto it = edges.begin();

        for (; it != edges.end() && it->ch() <= ch; ++it)
            if (it->ch() == ch)
                return insert(&*it, s, suffix);

        Trie_node* new_node = new Trie_node;
        new_node->m_results.push_back(s);

        edges.insert(it, {new_node, suffix});
    }

    Trie_node* find(std::string s) { return find(&m_root, (char*)(&s[0])); }

    Trie_node* find(Trie_node_edge* edge, char* suffix)
    {
        char* suf = suffix;
        char* ed_suf = &edge->m_suffix[0];

        while (*suf && *ed_suf && *suf == *ed_suf)
            ++suf, ++ed_suf;

        if (!*suf)
            return edge->m_node;

        if (!*ed_suf)
            return find(edge->m_node, suf);

        return nullptr;
    }

    Trie_node* find(Trie_node* node, char* suffix)
    {
        const char ch = *suffix;
        if (ch == '\0')
            return node;

        auto& edges = node->m_edges;
        for (auto it = edges.begin(); it != edges.end() && it->ch() <= ch; ++it)
            if (it->ch() == ch)
                return find(&*it, suffix);

        return nullptr;
    }

public:
    Trie_node m_root;
    std::vector<std::string> m_strings;
};

// NOLINTEND

#endif
