// Implementation of an adaptive radix tree based on paper
// The Adaptive Radix Tree: ARTful Indexing for Main-Memory Databases by Viktor Leis, Alfons Kemper,
// Thomas Neumann (https://db.in.tum.de/~leis/papers/ART.pdf)
//
#ifndef ART_H
#define ART_H

#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <string>

// NOLINTBEGIN

constexpr size_t CFG_node16_shrink_threshold = 2;
constexpr size_t CFG_node48_shrink_threshold = 14;
constexpr size_t CFG_node256_shrink_threshold = 46;

enum Node_t : uint8_t {
    Node4_t,
    Node16_t,
    Node48_t,
    Node256_t,
};

class Node;
class Node4;
class Node16;
class Node48;
class Node256;
class Leaf;

// Helper class that wraps key used in art.
// Data is not physically copied on construction to increase performance. This object just holds
// pointer to data beeing manipulated, it's size and current depth used for single value comparison
// on each traversed level. Depth is incresed by 1 for every new level and value at that depth can
// be accessed with operator[].
//
// In order to have multiple entries where one entry is prefix of another, we will insert invisible
// 0 at the end of a key, and increase it's size by 1. That way, all keys will have unique path
// through the tree. Typical additional character is $ for suffix trees, but we will use 0 for
// general purpose.
//
class Key {
public:
    friend class Leaf;
    static constexpr uint8_t term_byte = 0;

    Key(const uint8_t* const data, size_t size) noexcept : m_data{data}, m_size{size + 1} {}

    uint8_t operator[](size_t idx) const noexcept
    {
        assert(idx < m_size);
        return idx == m_size - 1 ? term_byte : m_data[idx];
    }

    size_t size() const noexcept { return m_size; }

    // Copy key to destination buffer with size len. We must manually copy last 0 byte, since we
    // don't hold that value in our buffer. User must assure that buffer can hold at least len
    // bytes.
    //
    void copy_to(uint8_t* dest, size_t len) const noexcept
    {
        std::memcpy(dest, m_data, std::min(len, m_size - 1));

        if (len >= m_size)
            dest[m_size - 1] = term_byte;
    }

private:
    const uint8_t* const m_data;
    size_t m_size;
};

// Pointer tagging.
// Since malloc's returned address is guaranteed to be aligned at least as std::max_align_t, we will
// use unused last bits in pointer to store additonal info. This can be used to simulate base class
// and virtual method calls.
//
static constexpr uintptr_t tag_bits = alignof(std::max_align_t) - 1;

constexpr uintptr_t raw(void* ptr) noexcept
{
    return std::bit_cast<uintptr_t>(ptr);
}

constexpr uintptr_t tag(void* ptr) noexcept
{
    return raw(ptr) & tag_bits;
}

constexpr void* clear_tag(void* ptr) noexcept
{
    return std::bit_cast<void*>(raw(ptr) & ~tag_bits);
}

constexpr void* set_tag(void* ptr, uintptr_t tag) noexcept
{
    assert((tag & ~tag_bits) == 0);
    return std::bit_cast<void*>(raw(clear_tag(ptr)) | (tag & tag_bits));
}

// Wrapper class that holds either pointer to node or pointer to leaf.
// It is not a smart pointer, hence user must manage memory manually.
//
class entry_ptr {
private:
    static constexpr uintptr_t node_tag = 0;
    static constexpr uintptr_t leaf_tag = 1;
    static_assert(leaf_tag <= tag_bits);

public:
    constexpr entry_ptr() noexcept : m_ptr{nullptr} {}

    constexpr entry_ptr(nullptr_t) noexcept : m_ptr{nullptr} {}

    constexpr entry_ptr(Node* node) noexcept : m_ptr{node} {}

    constexpr entry_ptr(Leaf* leaf) noexcept : m_ptr{set_tag(leaf, leaf_tag)} {}

    constexpr operator bool() const noexcept { return m_ptr != nullptr; }

    [[nodiscard]] constexpr bool leaf() const noexcept { return tag(m_ptr) == leaf_tag; }

    [[nodiscard]] constexpr bool node() const noexcept { return tag(m_ptr) == node_tag; }

    [[nodiscard]] constexpr Node* node_ptr() const noexcept
    {
        assert(node());
        return static_cast<Node*>(m_ptr);
    }

    [[nodiscard]] constexpr Leaf* leaf_ptr() const noexcept
    {
        assert(leaf());
        return static_cast<Leaf*>(clear_tag(m_ptr));
    }

    [[nodiscard]] constexpr const Leaf& next_leaf() const noexcept;

    constexpr void reset() noexcept;

private:
    void* m_ptr;
};

// Common node (header) used in all nodes. It holds prefix length, node type, number of children
// and a prefix. Prefix and prefix length are common for all nodes in a subtree. Prefix length
// can be greater than max_prefix_len, because we can only store 10 bytes in a prefix array.
// Because of this, we first need to compare prefix len bytes from prefix array, and if they all
// match and prefix_len > max_prefix_len, we must go to leaf to compare rest of bytes to ensure
// that we have a match. Because of this, we always split nodes on real prefix missmatch, not on
// max_prefix_len that can fit into this header. This is called a hybrid approach in ART -
// Section: III. ADAPTIVE RADIX TREE, E. Collapsing Inner Nodes
//
class Node {
public:
    static constexpr uint32_t max_prefix_len = 9;

    Node(const Node& other, uint8_t new_type);

    Node(uint8_t type) noexcept : m_type{type} {}

    void add_child(const uint8_t key, entry_ptr child) noexcept;
    void remove_child(const uint8_t key) noexcept;

    [[nodiscard]] bool full() const noexcept;
    [[nodiscard]] bool should_shrink() const noexcept;
    [[nodiscard]] bool should_collapse() const noexcept;

    [[nodiscard]] entry_ptr* find_child(uint8_t key) noexcept;

    [[nodiscard]] Node* grow() noexcept;
    [[nodiscard]] Node* shrink() noexcept;
    [[nodiscard]] entry_ptr collapse() noexcept;

    [[nodiscard]] const Leaf& next_leaf() const noexcept;

    [[nodiscard]] size_t common_header_prefix(const Key& key, size_t depth) const noexcept;
    [[nodiscard]] size_t common_prefix(const Key& key, size_t depth) const noexcept;

    uint32_t m_prefix_len = 0;
    uint16_t m_num_children = 0;
    uint8_t m_type;
    uint8_t m_prefix[max_prefix_len];
};

static_assert(sizeof(Node) == 16);

// Limits provided length with maximum header length and returns it.
//
template<typename Type>
static inline uint32_t hdrlen(Type len)
{
    return std::min(static_cast<uint32_t>(len), Node::max_prefix_len);
}

// Node4: The smallest node type can store up to 4 child pointers and uses an array of length 4
// for keys and another array of the same length for pointers. The keys and pointers are stored
// at corresponding positions and the keys are sorted.
//
class Node4 final : public Node {
public:
    Node4(const Key& key, void* value, size_t depth, Leaf* leaf);
    Node4(const Key& key, void* value, size_t depth, Node* node, size_t cp_len);

    Node4(Node16& old_node) noexcept;

    [[nodiscard]] entry_ptr collapse() noexcept;

    [[nodiscard]] entry_ptr* find_child(uint8_t key) noexcept;
    [[nodiscard]] Node16* grow() noexcept;

    [[nodiscard]] bool full() const noexcept { return m_num_children == 4; }

    [[nodiscard]] bool should_shrink() const noexcept { return false; };

    void add_child(const uint8_t key, entry_ptr child) noexcept;
    void remove_child(const uint8_t key) noexcept;

    [[nodiscard]] const Leaf& next_leaf() const noexcept;

    uint8_t m_keys[4]{};       // Keys with span of 1 byte.
    entry_ptr m_children[4]{}; // Pointers to children nodes.
};

// Node16: This node type is used for storing between 5 and 16 child pointers. Like the Node4,
// the keys and pointers are stored in separate arrays at corresponding positions, but both
// arrays have space for 16 entries. A key can be found efficiently with binary search or, on
// modern hardware, with parallel comparisons using SIMD instructions.
//
class Node16 final : public Node {
public:
    Node16(Node4& old_node) noexcept;
    Node16(Node48& old_node) noexcept;

    // TODO: Check if compiler generates SIMD instructions. If not, insert them manually.
    //
    [[nodiscard]] entry_ptr* find_child(uint8_t key) noexcept;
    [[nodiscard]] Node48* grow() noexcept;
    [[nodiscard]] Node4* shrink() noexcept;

    [[nodiscard]] bool full() const noexcept { return m_num_children == 16; }

    [[nodiscard]] bool should_shrink() const noexcept
    {
        return m_num_children == CFG_node16_shrink_threshold;
    };

    void add_child(const uint8_t key, entry_ptr child) noexcept;
    void remove_child(const uint8_t key) noexcept;

    [[nodiscard]] const Leaf& next_leaf() const noexcept;

    uint8_t m_keys[16]{};       // Keys with span of 1 byte.
    entry_ptr m_children[16]{}; // Pointers to children nodes.
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

    Node48(Node16& old_node) noexcept;
    Node48(Node256& old_node) noexcept;

    [[nodiscard]] entry_ptr* find_child(uint8_t key) noexcept;
    [[nodiscard]] Node256* grow() noexcept;
    [[nodiscard]] Node16* shrink() noexcept;

    [[nodiscard]] bool full() const noexcept { return m_num_children == 48; }

    [[nodiscard]] bool should_shrink() const noexcept
    {
        return m_num_children == CFG_node48_shrink_threshold;
    };

    void add_child(const uint8_t key, entry_ptr child) noexcept;
    void remove_child(const uint8_t key) noexcept;

    [[nodiscard]] const Leaf& next_leaf() const noexcept;

    // Keys with span of 1 byte with value of index for m_idxs array.
    uint8_t m_idxs[256];
    entry_ptr m_children[48]{}; // Pointers to children nodes.
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
    Node256(const Node48& old_node) noexcept;

    [[nodiscard]] entry_ptr* find_child(uint8_t key) noexcept;
    [[nodiscard]] Node48* shrink() noexcept;

    [[nodiscard]] bool full() const noexcept { return m_num_children == 256; }

    [[nodiscard]] bool should_shrink() const noexcept
    {
        return m_num_children == CFG_node256_shrink_threshold;
    };

    void add_child(const uint8_t key, entry_ptr child) noexcept;
    void remove_child(const uint8_t key) noexcept;

    [[nodiscard]] const Leaf& next_leaf() const noexcept;

    entry_ptr m_children[256]{}; // Pointers to children nodes.
};

class Leaf final {
public:
    // Creates new leaf from provided key. It allocates memory big enough to fit leaf object and
    // calls constructor on that memory. Allocation size for key is taken as a provided key size
    // and a depth that we are at, because we are only saving partial key from current depth.
    //
    static Leaf* new_leaf(const Key& key, void* value)
    {
        Leaf* leaf = static_cast<Leaf*>(std::malloc(sizeof(*leaf) + key.size()));

        leaf->m_value = value;
        leaf->m_key_len = key.size();
        key.copy_to(leaf->m_key, key.size());

        return leaf;
    }

    const uint8_t& operator[](size_t idx) const noexcept
    {
        assert(idx < m_key_len);
        return m_key[idx];
    }

    size_t size() const noexcept { return m_key_len; }

    // Returns whether internal key and provided key matches. Since internal key already holds
    // terminal byte at the end and key doesn't, we must memcmp all except last byte.
    //
    bool match(const Key& key) const noexcept
    {
        if (m_key_len != key.size())
            return false;

        return !std::memcmp(m_key, key.m_data, m_key_len - 1);
    }

    std::string key_to_string() const noexcept { return std::string(m_key, m_key + m_key_len - 1); }

    // Key key_to_key() const noexcept { return std::string(m_key, m_key + m_key_len - 1); }

public:
    void* m_value = nullptr;
    size_t m_key_len;
    uint8_t m_key[];
};

// Frees memory based on pointer type and sets pointer to nullptr.
//
constexpr void entry_ptr::reset() noexcept
{
    if (!*this)
        return;

    if (node()) {
        Node* n = node_ptr();
        switch (n->m_type) { // clang-format off
        case Node4_t:   delete (Node4*)  n; break;
        case Node16_t:  delete (Node16*) n; break;
        case Node48_t:  delete (Node48*) n; break;
        case Node256_t: delete (Node256*)n; break;
        default: assert(!"Invalid case.");  break;
        } // clang-format on
    }
    else
        delete leaf_ptr();

    m_ptr = nullptr;
}

class ART final {
public:
    void insert(const std::string& s, void* value = nullptr) noexcept
    {
        const Key key{(const uint8_t* const)s.data(), s.size()};
        insert(key, value);
    }

    void insert(const uint8_t* const data, size_t size, void* value = nullptr) noexcept
    {
        const Key key{data, size};
        insert(key, value);
    }

    void erase(const std::string& s) noexcept
    {
        const Key key{(const uint8_t* const)s.data(), s.size()};
        erase(key);
    }

    void erase(const uint8_t* const data, size_t size) noexcept
    {
        const Key key{data, size};
        erase(key);
    }

    Leaf* search(const std::string& s) noexcept
    {
        const Key key{(const uint8_t* const)s.data(), s.size()};
        return search(key);
    }

    Leaf* search(const uint8_t* const data, size_t size) noexcept
    {
        const Key key{data, size};
        return search(key);
    }

    template<bool include_keys = true>
    size_t physical_size() const noexcept;

private:
    void insert(const Key& key, void* value) noexcept;
    void insert(entry_ptr& entry, const Key& key, void* value, size_t depth) noexcept;

    void erase(const Key& key) noexcept;
    void erase(entry_ptr& entry, const Key& key, size_t depth) noexcept;

    void erase_leaf(entry_ptr& entry, Leaf* leaf, const Key& key, size_t depth) noexcept;

    Leaf* search(const Key& key) noexcept;
    Leaf* search(entry_ptr& entry, const Key& key, size_t depth) noexcept;

    void insert_at_leaf(entry_ptr& entry, const Key& key, void* value, size_t depth) noexcept;
    void insert_at_node(entry_ptr& entry, const Key& key, void* value, size_t depth,
                        size_t cp_len) noexcept;

    bool empty() const noexcept { return m_root; }

    template<bool include_keys>
    size_t physical_size(const entry_ptr& entry) const noexcept;

    entry_ptr m_root;
};

// NOLINTEND

#endif // ART_H
