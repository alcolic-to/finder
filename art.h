// Implementation of an adaptive radix tree based on paper
// The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases by Viktor Leis, Alfons Kemper,
// Thomas Neumann (https://db.in.tum.de/~leis/papers/ART.pdf)
//
#ifndef ART_H
#define ART_H

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>

// NOLINTBEGIN

// Helper class that wraps key used in art.
// It holds pointer to data beeing manipulated, it's size and current depth used for single value
// comparison on each traversed level. Depth is incresed by 1 for every new level and value at that
// depth can be accessed with operator[0].
//
// In order to have multiple entries where one entry is prefix of another, we will insert invisible
// 0 at the end of a key, and increase it's size by 1. That way, all keys will have unique path
// through the tree. Typical additional character in theory is $ for suffix trees, but we will use 0
// for general purpose.
//

class Key {
public:
    static constexpr uint8_t term_byte = 0;
    friend class Leaf;

    Key(uint8_t* data, size_t size) noexcept : m_data{data}, m_size{size + 1} {}

    Key(Leaf& leaf) noexcept;

    uint8_t operator[](size_t idx) const noexcept
    {
        size_t abs_offset = m_depth + idx;
        assert(abs_offset < m_size);

        return abs_offset == m_size - 1 ? term_byte : m_data[abs_offset];
    }

    Key& operator++() noexcept
    {
        assert(m_depth < m_size);
        ++m_depth;
        return *this;
    }

    [[nodiscard]] bool end() const noexcept { return m_depth == m_size; }

    void copy_to(uint8_t* dest, size_t len) const noexcept;

private:
    uint8_t* m_data;
    size_t m_size;
    size_t m_depth = 0;
};

// Pointer tagging. We will use last 2 bits for tag.

static constexpr uintptr_t tag_bits = 0b11;

inline uintptr_t raw(void* ptr) noexcept
{
    return uintptr_t(ptr);
}

inline uintptr_t tag(void* ptr) noexcept
{
    return raw(ptr) & tag_bits;
}

inline void* clear_tag(void* ptr) noexcept
{
    return (void*)(raw(ptr) & ~tag_bits);
}

inline void* set_tag(void* ptr, uintptr_t tag) noexcept
{
    return (void*)(raw(clear_tag(ptr)) | tag);
}

class Node;
class Leaf;

// Wrapper class that holds either pointer to node or pointer to leaf.
//
class entry_ptr {
public:
    entry_ptr() noexcept : m_ptr{nullptr} {}

    entry_ptr(Node* node) noexcept : m_ptr{node} {}

    entry_ptr(Leaf* leaf) noexcept : m_ptr{set_tag(leaf, leaf_tag)} {}

    // Copy, move, delete.
    //
    entry_ptr(const entry_ptr& other) noexcept : m_ptr{other.m_ptr} {};

    entry_ptr& operator=(const entry_ptr& other) noexcept
    {
        m_ptr = other.m_ptr;
        return *this;
    };

    entry_ptr(entry_ptr&& other) noexcept : m_ptr{other.m_ptr} { other.m_ptr = nullptr; };

    entry_ptr& operator=(entry_ptr&& other) noexcept
    {
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        return *this;
    };

    ~entry_ptr() noexcept;

    operator void*() const noexcept { return clear_tag(m_ptr); }

    operator bool() const noexcept { return bool(clear_tag(m_ptr)); }

    [[nodiscard]] bool leaf() const noexcept { return tag(m_ptr) == leaf_tag; }

    [[nodiscard]] bool node() const noexcept { return tag(m_ptr) == node_tag; }

    [[nodiscard]] Node* node_ptr() const noexcept
    {
        assert(node());
        return (Node*)m_ptr;
    }

    [[nodiscard]] Leaf* leaf_ptr() const noexcept
    {
        assert(leaf());
        return (Leaf*)clear_tag(m_ptr);
    }

private:
    static constexpr uintptr_t node_tag = 0;
    static constexpr uintptr_t leaf_tag = 1;

    void* m_ptr;
};

// clang-format off

enum Node_t : uint32_t {
    Node4_t   = 0b000,
    Node16_t  = 0b001,
    Node48_t  = 0b010,
    Node256_t = 0b011,
    Leaf_t    = 0b100,
};

// clang-format on

// constexpr uint32_t node_t_bits = 0b11100000'00000000'00000000'00000000; // >> 29
// constexpr uint32_t node_t_bits_offset = std::countr_zero(node_t_bits);

// constexpr uint32_t num_children_bits = 0b00000000'00000000'00000000'11111111;
// constexpr uint32_t num_children_bits_offset = std::countr_zero(num_children_bits);

// constexpr uint32_t prefix_len_bits = 0b00000000'00000000'11111111'00000000;
// constexpr uint32_t prefix_len_bits_offset = std::countr_zero(prefix_len_bits);

// constexpr uint32_t key_len_bits = 0b00011111'11111111'11111111'11111111;
// constexpr uint32_t key_len_bits_offset = std::countr_zero(key_len_bits);

// // clang-format on

// // 4 bytes for header!!!
// //

// // Node header.
// // Contains different info based on node type (inner node or a leaf).
// // For inner node, we are storring node type, number of children and prefix len.
// // For leaf node, we are storring node type and key len.
// //
// struct Node_header {
//     uint32_t m_bytes;
// };

// struct nh {
//     uint8_t m_flags;
//     uint8_t num_childs;
//     uint8_t m_prefix[14]; // -> 14 bytes for prefix.
// };

// struct lh {
//     void* data;
//     uint32_t key_len;
//     uint8_t key[];
// };

// // Node header. First 3 bits in flags represents node
// // type and the 3rd bit represents whether we have compresses path or not.
// //
// struct Node_header {
//     Node_header() = default;

//     Node_header(Node_t type, bool comp = false)
//         : m_flags{uint8_t(type | (comp * Bits::compression))}
//     {
//     }

//     Node_t type() { return Node_type(m_flags & Bits::type); }

//     void set_type(Node_t type) { m_flags = type | (m_flags & ~Bits::type); }

//     bool compressed() { return bool(m_flags & Bits::compression); }

//     void set_compression(bool comp)
//     {
//         m_flags = (comp * Bits::compression) | (m_flags & ~Bits::compression);
//     }

//     bool leaf() { return m_flags & Bits::leaf; }

//     uint8_t m_flags = 0;
// };

class Node {
public:
    static constexpr uint8_t max_prefix_len = 13;

    Node(uint8_t type, uint8_t num_children = 0, uint8_t prefix_len = 0,
         uint8_t* prefix = nullptr) noexcept
        : m_type{type}
        , m_num_children{num_children}
        , m_prefix_len{prefix_len}
    {
        if (prefix != nullptr)
            std::memcpy(m_prefix, prefix, std::min(max_prefix_len, m_prefix_len));
    }

    [[nodiscard("Do not discard result, because node can be resized and new pointer value must "
                "be taken.")]]
    Node* add_child(const Key& key) noexcept;

    [[nodiscard]] entry_ptr find_child(uint8_t key) noexcept;

    uint8_t m_type;
    uint8_t m_num_children;
    uint8_t m_prefix_len;
    uint8_t m_prefix[max_prefix_len];
};

class Node16;
class Node48;
class Node256;

// Node4: The smallest node type can store up to 4 child pointers and uses an array of length 4
// for keys and another array of the same length for pointers. The keys and pointers are stored
// at corresponding positions and the keys are sorted.
//
class Node4 final : public Node {
public:
    Node4() : Node{Node4_t} {}

    [[nodiscard]] entry_ptr find_child(uint8_t key) const noexcept;
    [[nodiscard]] Node16* grow() noexcept;
    [[nodiscard]] bool full() const noexcept;

    [[nodiscard(
        "Do not discard result, because node can be resized and new pointer value must be taken.")]]
    Node* add_child(const Key& key) noexcept;

    uint8_t m_keys[4] = {};       // Keys with span of 1 byte.
    entry_ptr m_children[4] = {}; // Pointers to children nodes.
};

// Node16: This node type is used for storing between 5 and 16 child pointers. Like the Node4,
// the keys and pointers are stored in separate arrays at corresponding positions, but both
// arrays have space for 16 entries. A key can be found efficiently with binary search or, on
// modern hardware, with parallel comparisons using SIMD instructions.
//
class Node16 final : public Node {
public:
    Node16() noexcept : Node{Node16_t} {}

    Node16(Node4& old_node) noexcept : Node{Node16_t, old_node.m_num_children}
    {
        // TODO: Copy prefix.
        std::memcpy(m_keys, old_node.m_keys, 4);
        std::memcpy(m_children, old_node.m_children, 4);
    }

    // TODO: Check if compiler generates SIMD instructions. If not, insert them manually.
    //
    [[nodiscard]] entry_ptr find_child(uint8_t key) const noexcept;
    [[nodiscard]] Node48* grow() noexcept;
    [[nodiscard]] bool full() const noexcept;

    [[nodiscard(
        "Do not discard result, because node can be resized and new pointer value must be taken.")]]
    Node* add_child(const Key& key) noexcept;

    uint8_t m_keys[16] = {};       // Keys with span of 1 byte.
    entry_ptr m_children[16] = {}; // Pointers to children nodes.
};

// Node48: As the number of entries in a node increases, searching the key array becomes
// expensive. Therefore, nodes with more than 16 pointers do not store the keys explicitly.
// Instead, a 256-element array is used, which can be indexed with key bytes directly. If a node
// has between 17 and 48 child pointers, this array stores indexes into a second array which
// contains up to 48 pointers. This indirection saves space in comparison to 256 pointers of 8
// bytes, because the indexes only require 6 bits (we use 1 byte for simplicity).
//
class Node48 final : public Node {
public:
    static constexpr uint8_t empty_slot = 255;

    Node48() noexcept : Node{Node48_t} {}

    [[nodiscard]] entry_ptr find_child(uint8_t key) const noexcept;
    [[nodiscard]] Node256* grow() noexcept;
    [[nodiscard]] bool full() const noexcept;

    [[nodiscard(
        "Do not discard result, because node can be resized and new pointer value must be taken.")]]
    Node* add_child(const Key& key) noexcept;

    // Keys with span of 1 byte with value of index for m_idxs array.
    uint8_t m_idxs[256] = {empty_slot};
    entry_ptr m_children[48] = {}; // Pointers to children nodes.
};

// Node256: The largest node type is simply an array of 256 pointers and is used for storing
// between 49 and 256 entries. With this representation, the next node can be found very
// efficiently using a single lookup of the key byte in that array. No additional indirection is
// necessary. If most entries are not null, this representation is also very space efficient
// because only pointers need to be stored. Additionally, at the front of each inner node, a
// header of constant size (e.g., 16 bytes) stores the node type, the number of children, and
// the compressed path (cf. Section III-E).
//
class Node256 final : public Node {
public:
    Node256() noexcept : Node{Node256_t} {}

    [[nodiscard]] entry_ptr find_child(uint8_t key) const noexcept;

    [[nodiscard(
        "Do not discard result, because node can be resized and new pointer value must be taken.")]]
    Node* add_child(const Key& key) noexcept;

    entry_ptr m_children[256] = {}; // Pointers to children nodes.
};

class Leaf final {
public:
    // Creates new leaf. All bytes from key at current depth until the key end are copied
    // to internal m_key buffer.
    //
    Leaf(const Key& key) : m_key_len{key.m_size} { key.copy_to(m_key, key.m_size); }

    // Creates new leaf from provided key. It allocates memory big enough to fit leaf object and
    // calls constructor on that memory. Allocation size for key is taken as a provided key size and
    // a depth that we are at, because we are only saving partial key from current depth.
    //
    static Leaf* new_leaf(const Key& key)
    {
        void* mem = std::malloc(sizeof(Leaf) + key.m_size);
        return new (mem) Leaf{key};
    }

    void* m_data = nullptr;
    size_t m_key_len;
    uint8_t m_key[];
};

class ART final {
public:
    void insert(uint8_t* data, size_t size) noexcept;

private:
    void insert(entry_ptr& entry, Key& key) noexcept;

    bool empty() const noexcept { return m_root; }

    entry_ptr m_root;
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
