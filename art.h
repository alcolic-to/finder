// Implementation of an adaptive radix tree based on paper
// The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases by Viktor Leis, Alfons Kemper,
// Thomas Neumann (https://db.in.tum.de/~leis/papers/ART.pdf)
//
#ifndef ART_H
#define ART_H

#include <cassert>
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

enum Bits : uint8_t {
    type =        0b0111,
    compression = 0b1000,
    leaf        = 0b0100
};

// clang-format on

// Node header. First 3 bits in flags represents node
// type and the 3rd bit represents whether we have compresses path or not.
//
struct Node_header {
    Node_header() = default;

    Node_header(Node_type type, bool comp = false)
        : m_flags{uint8_t(type | (comp * Bits::compression))}
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

    uint8_t m_flags = 0;
};

class Node;
typedef Node* node_ptr;

// Class that only defines functions used by nodes. It only holds pointer to vtable.
//
class Node {
public:
    virtual node_ptr find_child(uint8_t key) = 0;
    virtual void grow() = 0;
};

class Inner_node : public Node {
public:
    Node_header m_header;
    uint8_t m_compressed_path[15] = {}; // Contains compressed key path.
};

// Node4: The smallest node type can store up to 4 child pointers and uses an array of length 4
// for keys and another array of the same length for pointers. The keys and pointers are stored
// at corresponding positions and the keys are sorted.
//
class Node4 : public Inner_node {
public:
    // Node4() : Node(Node4_t, false), m_compressed_path{} {}

    node_ptr find_child(uint8_t key) override;

    void grow() override;

    uint8_t m_keys[4] = {};   // Keys with span of 1 byte.
    node_ptr m_nodes[4] = {}; // Pointers to children nodes.
};

// Node16: This node type is used for storing between 5 and 16 child pointers. Like the Node4,
// the keys and pointers are stored in separate arrays at corresponding positions, but both
// arrays have space for 16 entries. A key can be found efficiently with binary search or, on
// modern hardware, with parallel comparisons using SIMD instructions.
//
class Node16 : public Inner_node {
public:
    // TODO: Check if compiler generates SIMD instructions. If not, insert them manually.
    //
    node_ptr find_child(uint8_t key) override;
    void grow() override;

    uint8_t m_keys[16] = {};   // Keys with span of 1 byte.
    node_ptr m_nodes[16] = {}; // Pointers to children nodes.
};

// Node48: As the number of entries in a node increases, searching the key array becomes
// expensive. Therefore, nodes with more than 16 pointers do not store the keys explicitly.
// Instead, a 256-element array is used, which can be indexed with key bytes directly. If a node
// has between 17 and 48 child pointers, this array stores indexes into a second array which
// contains up to 48 pointers. This indirection saves space in comparison to 256 pointers of 8
// bytes, because the indexes only require 6 bits (we use 1 byte for simplicity).
//
class Node48 : Inner_node {
public:
    static constexpr uint8_t empty = -1;

    node_ptr find_child(uint8_t key) override;

    void grow() override;

    uint8_t m_keys[256] = {empty}; // Keys with span of 1 byte with value of index for m_idxs array.
    node_ptr m_nodes[48] = {};     // Pointers to children nodes.
};

// Node256: The largest node type is simply an array of 256 pointers and is used for storing
// between 49 and 256 entries. With this representation, the next node can be found very
// efficiently using a single lookup of the key byte in that array. No additional indirection is
// necessary. If most entries are not null, this representation is also very space efficient
// because only pointers need to be stored. Additionally, at the front of each inner node, a
// header of constant size (e.g., 16 bytes) stores the node type, the number of children, and
// the compressed path (cf. Section III-E).
//
class Node256 : Inner_node {
public:
    node_ptr find_child(uint8_t key) override;

    void grow() override;

    node_ptr m_nodes[256] = {}; // Pointers to children nodes.
};

class Leaf256 : Node {
public:
    node_ptr find_child(uint8_t key) { return assert(!"Invalid call."), nullptr; };

    void grow() { assert(!"Invalid call."); }

    Node_header m_header{Leaf256_t};
    uint8_t m_data[255] = {};
};

node_ptr Node4::find_child(uint8_t key)
{
    for (int i = 0; i < 4; ++i)
        if (m_keys[i] == key)
            return m_nodes[i];

    return nullptr;
}

void Node4::grow()
{
    Node16* new_node = new Node16{};
    for (int i = 0; i < 4; ++i) {
    }
}

node_ptr Node16::find_child(uint8_t key)
{
    for (int i = 0; i < 16; ++i)
        if (m_keys[i] == key)
            return m_nodes[i];

    return nullptr;
}

void Node16::grow() {}

node_ptr Node48::find_child(uint8_t key)
{
    return m_keys[key] != empty ? m_nodes[m_keys[key]] : nullptr;
}

void Node48::grow() {}

node_ptr Node256::find_child(uint8_t key)
{
    return m_nodes[key];
}

void Node256::grow()
{
    assert(!"Invalid call.");
}

class ART {
public:
    void insert(uint8_t* data, size_t size);
    void insert(node_ptr& node, uint8_t* data, size_t depth);

    bool empty() const noexcept { return m_root != nullptr; }

    node_ptr m_root;
};

// For insert, this part of code makes new node with 2 children: 1. new key that is beeing inserted
// and the second which is an existing node. Because we are holding prefix in every node, common
// prefix is extracted from both nodes and inserted into prefix array. This way we are always
// getting inner node with two child nodes.

/*
 4 if isLeaf(node) //expandnode
 5 newNode=makeNode4()
 6 key2=loadKey(node)
 7 for(i=depth;key[i]==key2[i];i=i+1)
 8 newNode.prefix[i-depth]=key[i]
 9 newNode.prefixLen=i-depth
 10 depth=depth+newNode.prefixLen
 11 addChild(newNode,key[depth],leaf)
 12 addChild(newNode,key2[depth],node)
 13 replace(node,newNode)
 14 return
 */

void ART::insert(uint8_t* data, size_t size)
{
    if (m_root == nullptr)
        m_root = (node_ptr) new Leaf256{};
    else
        insert(m_root, data, size);
}

void lazy_expand();

void ART::insert(node_ptr& node, uint8_t* key, size_t depth)
{
    // if (node->leaf())
    // lazy_expand();

    node->find_child(*key);
}

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
