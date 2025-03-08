// Implementation of an adaptive radix tree based on paper
// The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases by Viktor Leis, Alfons Kemper,
// Thomas Neumann (https://db.in.tum.de/~leis/papers/ART.pdf)
//
#ifndef ART_H
#define ART_H

#pragma once

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

// clang-format off
enum Bits : uint8_t {
    type =        0b00111,
    compression = 0b01000,
    leaf =        0b10000
};

enum Node_type : uint8_t {
    Node4_t   = 0b000,
    Node16_t  = 0b001,
    Node48_t  = 0b010,
    Node256_t = 0b011,
    Leaf4_t   = 0b100,
    Leaf16_t  = 0b101,
    Leaf48_t  = 0b110,
    Leaf256_t = 0b111
};

// clang-format on

// Node header. First 3 bits in flags represents node
// type and the 3rd bit represents whether we have compresses path or not and the third bit
// represents whether node is a leaf.
//
struct Node_header {
    Node_header() = default;

    Node_header(Node_type type, bool comp = false, bool leaf = false)
        : m_flags{uint8_t(type | (comp * Bits::compression) | (leaf * Bits::leaf))}
    {
    }

    Node_type type() { return Node_type(m_flags & Bits::type); }

    void set_type(Node_type type) { m_flags = type | (m_flags & ~Bits::type); }

    bool compressed() { return bool(m_flags & Bits::compression); }

    void set_compression(bool comp)
    {
        m_flags = (comp * Bits::compression) | (m_flags & ~Bits::compression);
    }

    bool leaf() { return m_flags & Bits::leaf; }

    void set_leaf(bool leaf) { m_flags = (leaf * Bits::leaf) | (m_flags & ~Bits::leaf); }

    uint8_t m_flags = 0;
};

// Node4: The smallest node type can store up to 4 child pointers and uses an array of length 4
// for keys and another array of the same length for pointers. The keys and pointers are stored
// at corresponding positions and the keys are sorted.
//
class Node4 {
public:
    Node_header m_header{Node4_t};
    uint8_t m_compressed_path[15]; // Contains compressed key path.
    uint8_t m_keys[4];             // Keys with span of 1 byte.
    Node_header* m_nodes[4];       // Pointers to children nodes.
};

// Node16: This node type is used for storing between 5 and 16 child pointers. Like the Node4,
// the keys and pointers are stored in separate arrays at corresponding positions, but both
// arrays have space for 16 entries. A key can be found efficiently with binary search or, on
// modern hardware, with parallel comparisons using SIMD instructions.
//
class Node16 {
public:
    Node_header m_header{Node16_t};
    uint8_t m_compressed_path[15]; // Contains compressed key path.
    uint8_t m_keys[16];            // Keys with span of 1 byte.
    Node_header* m_nodes[16];      // Pointers to children nodes.
};

// Node48: As the number of entries in a node increases, searching the key array becomes
// expensive. Therefore, nodes with more than 16 pointers do not store the keys explicitly.
// Instead, a 256-element array is used, which can be indexed with key bytes directly. If a node
// has between 17 and 48 child pointers, this array stores indexes into a second array which
// contains up to 48 pointers. This indirection saves space in comparison to 256 pointers of 8
// bytes, because the indexes only require 6 bits (we use 1 byte for simplicity).
//
class Node48 {
public:
    Node_header m_header{Node48_t};
    uint8_t m_compressed_path[15]; // Contains compressed key path.
    uint8_t m_keys[256];           // Keys with span of 1 byte with value of index for m_idxs array.
    Node_header* m_nodes[48];      // Pointers to children nodes.
};

// Node256: The largest node type is simply an array of 256 pointers and is used for storing
// between 49 and 256 entries. With this representation, the next node can be found very
// efficiently using a single lookup of the key byte in that array. No additional indirection is
// necessary. If most entries are not null, this representation is also very space efficient
// because only pointers need to be stored. Additionally, at the front of each inner node, a
// header of constant size (e.g., 16 bytes) stores the node type, the number of children, and
// the compressed path (cf. Section III-E).
//
class Node256 {
public:
    Node_header m_header{Node256_t};
    uint8_t m_compressed_path[15]; // Contains compressed key path.
    Node_header* m_nodes[256];     // Pointers to children nodes.
};

class Leaf256 {
public:
    Node_header m_header{Leaf256_t};
    uint8_t m_data[256];
};

class ART {
public:
    ART() { m_root = (Node_header*)new Leaf256{}; }

    bool empty() const noexcept { return m_root != nullptr; }

    Node_header* m_root;
};

// Some aditional code that might me used in future:
//
// Idea: when node type is determined, we can just return an index of a node type and call a
// function of that node type with corresponding pointer.
//
// arr[4] = { Node4*, Node16*, Node48*, Node64* };
// arr[node_type()]->find() or something like that.
// Benchmark that approach with simle if/else approach.

// class Node_pool {
// public:
// Node_pool(size_t size = 1024)
// {
// m_node4s.reserve(size);
// m_node16s.reserve(size);
// m_node48s.reserve(size);
// m_node256s.reserve(size);
// }

// std::vector<Node4> m_node4s;
// std::vector<Node16> m_node16s;
// std::vector<Node48> m_node48s;
// std::vector<Node256> m_node256s;
// };

// class Node_idx {
// public:
// void* operator[](size_t idx)
// {
// Node_type type = Node_type(m_node_type);
// switch (type) {
// case Node4_t:
// return
// }
// }

// uint64_t m_node_type : 2;
// uint64_t m_idx : 62;
// };

// static_assert(sizeof(Node_idx) == 8);

// NOLINTEND

#endif // ART_H
