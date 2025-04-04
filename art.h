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
#include <cstring>
#include <string>

// NOLINTBEGIN

namespace art {

constexpr size_t CFG_node16_shrink_threshold = 2;
constexpr size_t CFG_node48_shrink_threshold = 14;
constexpr size_t CFG_node256_shrink_threshold = 46;

enum Node_t : uint8_t {
    Node4_t,
    Node16_t,
    Node48_t,
    Node256_t,
};

template<class T>
class Node;

template<class T>
class Node4;

template<class T>
class Node16;

template<class T>
class Node48;

template<class T>
class Node256;

template<class T>
class Leaf;

static constexpr uint32_t max_prefix_len = 9;

// Limits provided length with maximum header length and returns it.
//
template<typename Type>
uint32_t hdrlen(Type len)
{
    return std::min(static_cast<uint32_t>(len), max_prefix_len);
}

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
    template<class T>
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
// In order to simplify assigning another pointer with freeing current resources, reset() funtion is
// implemented.
//
template<class T>
class entry_ptr {
public:
    using Node = Node<T>;
    using Leaf = Leaf<T>;

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

    // Returns next leaf from current entry. If current entry is leaf, it returns it. Otherwise, it
    // calls next_leaf() on a node which will recurse down one level based on node type an call us
    // again.
    //
    [[nodiscard]] constexpr const Leaf& next_leaf() const noexcept
    {
        assert(*this);
        return leaf() ? *leaf_ptr() : node_ptr()->next_leaf();
    }

    // Frees memory based on pointer type and sets pointer to other (nullptr by default).
    //
    constexpr void reset(entry_ptr other = nullptr) noexcept
    {
        if (*this) {
            if (node()) {
                Node* n = node_ptr();
                switch (n->m_type) { // clang-format off
                case Node4_t:   delete n->node4();   break;
                case Node16_t:  delete n->node16();  break;
                case Node48_t:  delete n->node48();  break;
                case Node256_t: delete n->node256(); break;
                default: assert(!"Invalid case.");   break;
                } // clang-format on
            }
            else {
                delete leaf_ptr();
            }
        }

        *this = other;
    }

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
template<class T>
class Node {
public:
    using Node4 = Node4<T>;
    using Node16 = Node16<T>;
    using Node48 = Node48<T>;
    using Node256 = Node256<T>;
    using Leaf = Leaf<T>;
    using entry_ptr = entry_ptr<T>;

    // Construct new node from previous node by copying whole header except node type which is
    // provided as new_type.
    //
    Node(const Node& other, uint8_t new_type)
        : m_prefix_len{other.m_prefix_len}
        , m_num_children{other.m_num_children}
        , m_type{new_type}
    {
        if (other.m_prefix_len > 0)
            std::memcpy(m_prefix, other.m_prefix, hdrlen(other.m_prefix_len));
    }

    Node(uint8_t type) noexcept : m_type{type} {}

    // Node pointer casts which simplifies node usage.
    // clang-format off
    //
    constexpr       Node4*   node4()         { assert(m_type == Node4_t);   return static_cast<      Node4*>  (this); }
    constexpr const Node4*   node4()   const { assert(m_type == Node4_t);   return static_cast<const Node4*>  (this); }
    constexpr       Node16*  node16()        { assert(m_type == Node16_t);  return static_cast<      Node16*> (this); }
    constexpr const Node16*  node16()  const { assert(m_type == Node16_t);  return static_cast<const Node16*> (this); }
    constexpr       Node48*  node48()        { assert(m_type == Node48_t);  return static_cast<      Node48*> (this); }
    constexpr const Node48*  node48()  const { assert(m_type == Node48_t);  return static_cast<const Node48*> (this); }
    constexpr       Node256* node256()       { assert(m_type == Node256_t); return static_cast<      Node256*>(this); }
    constexpr const Node256* node256() const { assert(m_type == Node256_t); return static_cast<const Node256*>(this); }
    //
    // clang-format on

    [[nodiscard]] entry_ptr* find_child(uint8_t key) noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t:   return node4()->find_child(key);
        case Node16_t:  return node16()->find_child(key);
        case Node48_t:  return node48()->find_child(key);
        case Node256_t: return node256()->find_child(key);
        default:        return assert(!"Invalid case."), nullptr;
        } // clang-format on
    }

    void add_child(const uint8_t key, entry_ptr child) noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t:   return node4()->add_child(key, child);
        case Node16_t:  return node16()->add_child(key, child);
        case Node48_t:  return node48()->add_child(key, child);
        case Node256_t: return node256()->add_child(key, child);
        default:        return assert(!"Invalid case.");
        } // clang-format on
    }

    void remove_child(const uint8_t key) noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t:   return node4()->remove_child(key);
        case Node16_t:  return node16()->remove_child(key);
        case Node48_t:  return node48()->remove_child(key);
        case Node256_t: return node256()->remove_child(key);
        default:        return assert(!"Invalid case.");
        } // clang-format on
    }

    [[nodiscard]] bool full() const noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t:   return node4()->full();
        case Node16_t:  return node16()->full();
        case Node48_t:  return node48()->full();
        case Node256_t: return node256()->full();
        default:        return assert(!"Invalid case."), false;
        } // clang-format on
    }

    [[nodiscard]] bool should_shrink() const noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t:   return node4()->should_shrink();
        case Node16_t:  return node16()->should_shrink();
        case Node48_t:  return node48()->should_shrink();
        case Node256_t: return node256()->should_shrink();
        default:        return assert(!"Invalid case."), false;
        } // clang-format on
    }

    [[nodiscard]] bool should_collapse() const noexcept
    {
        return m_type == Node4_t && m_num_children == 1;
    }

    [[nodiscard]] Node* grow() noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t:  return node4()->grow();
        case Node16_t: return node16()->grow();
        case Node48_t: return node48()->grow();
        default:       return assert(!"Invalid case."), this;
        } // clang-format on
    }

    [[nodiscard]] Node* shrink() noexcept
    {
        switch (m_type) { // clang-format off
        case Node16_t:  return node16()->shrink();
        case Node48_t:  return node48()->shrink();
        case Node256_t: return node256()->shrink();
        default:        return assert(!"Invalid case."), this;
        } // clang-format on
    }

    [[nodiscard]] entry_ptr collapse() noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t: return node4()->collapse();
        default:      return assert(!"Invalid case."), this;
        } // clang-format on
    }

    [[nodiscard]] const Leaf& next_leaf() const noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t:   return node4()->next_leaf();
        case Node16_t:  return node16()->next_leaf();
        case Node48_t:  return node48()->next_leaf();
        case Node256_t: return node256()->next_leaf();
        default:        return assert(!"Invalid case."), *Leaf::new_leaf(Key{nullptr, 0}, nullptr);
        } // clang-format on
    }

    // Returns common prefix len for key at current depth and a prefix in our header.
    // Header must not hold terminal byte, so we are comparing to minimal of node prefix len and max
    // prefix len. If depth reaches key.size() - 1, we will get terminal byte and stop comparing.
    //
    [[nodiscard]] size_t common_header_prefix(const Key& key, size_t depth) const noexcept
    {
        const size_t max_cmp = hdrlen(m_prefix_len);
        size_t cp_len = 0;

        while (cp_len < max_cmp && key[depth + cp_len] == m_prefix[cp_len])
            ++cp_len;

        return cp_len;
    }

    // Returns common prefix length from this node to the leaf and a key at provided depth.
    // If prefix in our header is max_prefix_len long and we've matched whole prefix, we need to
    // find leaf to keep comparing. It is not important which leaf we take, because all of them have
    // at least m_prefix_len common bytes.
    //
    [[nodiscard]] size_t common_prefix(const Key& key, size_t depth) const noexcept
    {
        size_t cp_len = common_header_prefix(key, depth);

        if (cp_len == max_prefix_len && cp_len < m_prefix_len) {
            const Leaf& leaf = next_leaf();
            while (cp_len < m_prefix_len && key[depth + cp_len] == leaf[depth + cp_len])
                ++cp_len;
        }

        return cp_len;
    }

    uint32_t m_prefix_len = 0;
    uint16_t m_num_children = 0;
    uint8_t m_type;
    uint8_t m_prefix[max_prefix_len];
};

static_assert(sizeof(Node<void>) == 16);

// Node4: The smallest node type can store up to 4 child pointers and uses an array of length 4
// for keys and another array of the same length for pointers. The keys and pointers are stored
// at corresponding positions and the keys are sorted.
//
template<class T>
class Node4 final : public Node<T> {
public:
    using Node = Node<T>;
    using Node16 = Node16<T>;
    using Node48 = Node48<T>;
    using Node256 = Node256<T>;
    using Leaf = Leaf<T>;
    using entry_ptr = entry_ptr<T>;

    using Node::m_num_children;
    using Node::m_prefix;
    using Node::m_prefix_len;
    using Node::m_type;

    // Creates new node4 as a parent for key(future leaf node) and leaf. New node will have 2
    // children with common prefix extracted from them. We will store information about common
    // prefix len in prefix_len, but we can only copy up to max_prefix_bytes in prefix array.
    // Later when searching for a key, we will compare all bytes from our prefix array, and if
    // all bytes matches, we will just skip all prefix bytes and go to directly to next node.
    // This is hybrid approach which mixes optimistic and pessimistic approaches.
    //
    Node4(const Key& key, void* value, size_t depth, Leaf* leaf) : Node{Node4_t}
    {
        for (m_prefix_len = 0; key[depth] == leaf->m_key[depth]; ++m_prefix_len, ++depth)
            if (m_prefix_len < max_prefix_len)
                m_prefix[m_prefix_len] = key[depth];

        assert(depth < key.size() && depth < leaf->m_key_len);
        assert(key[depth] != leaf->m_key[depth]);

        add_child(key[depth], Leaf::new_leaf(key, value));
        add_child(leaf->m_key[depth], leaf);
    }

    // Creates new node4 as a parent for key(future leaf node) and node. New node will have 2
    // children with common prefix extracted from them. We must also delete taken prefix from child
    // node, plus we must take one additional byte from prefix as a key for child node.
    // If node prefix does not fit into header, we must go to leaf node (not important which one),
    // to take prefix bytes from there. It it fits, we can take bytes from header directly.
    //
    Node4(const Key& key, void* value, size_t depth, Node* node, size_t cp_len) : Node{Node4_t}
    {
        assert(cp_len < node->m_prefix_len);

        m_prefix_len = cp_len;
        std::memcpy(m_prefix, node->m_prefix, hdrlen(cp_len));

        const uint8_t* prefix_src = node->m_prefix_len <= max_prefix_len ?
                                        &node->m_prefix[cp_len] :
                                        &(node->next_leaf()[depth + cp_len]);

        const uint8_t node_key = prefix_src[0]; // Byte for node mapping.

        node->m_prefix_len -= (cp_len + 1); // Additional byte for node mapping.
        std::memmove(node->m_prefix, prefix_src + 1, hdrlen(node->m_prefix_len));

        assert(node_key != key[depth + cp_len]);

        add_child(node_key, node);
        add_child(key[depth + cp_len], Leaf::new_leaf(key, value));
    }

    Node4(Node16& old_node) noexcept : Node{old_node, Node4_t}
    {
        std::memcpy(m_keys, old_node.m_keys, old_node.Node::m_num_children);
        std::memcpy(m_children, old_node.m_children, old_node.m_num_children * sizeof(entry_ptr));
    }

    [[nodiscard]] entry_ptr* find_child(uint8_t key) noexcept
    {
        for (int i = 0; i < m_num_children; ++i)
            if (m_keys[i] == key)
                return &m_children[i];

        return nullptr;
    }

    // Adds new child to node.
    // Finds place for new child (while keeping sorted order) and moves all children by 1 place to
    // make space for new child. Inserts new child after that.
    //
    void add_child(const uint8_t key, entry_ptr child) noexcept
    {
        assert(m_num_children < 4);

        size_t idx = 0;
        while (idx < m_num_children && m_keys[idx] < key)
            ++idx;

        for (size_t i = m_num_children; i > idx; --i) {
            m_keys[i] = m_keys[i - 1];
            m_children[i] = m_children[i - 1];
        }

        m_keys[idx] = key;
        m_children[idx] = child;

        ++m_num_children;
    }

    void remove_child(const uint8_t key) noexcept
    {
        size_t idx = 0;
        while (m_keys[idx] != key)
            ++idx;

        assert(idx < m_num_children);
        m_children[idx].reset();

        for (; idx < m_num_children - 1; ++idx) {
            m_keys[idx] = m_keys[idx + 1];
            m_children[idx] = m_children[idx + 1];
        }

        m_keys[idx] = 0;
        m_children[idx] = nullptr;

        --m_num_children;
    }

    // Collapses node4 by replacing it with child entry (which is the only child in this node). This
    // process is reverse of Node4 creation from 2 children entries.
    // If child entry is a node, all bytes that can fit in child's header are copied (key for
    // mapping first and header prefix after). Since we must use prefix bytes from our header as
    // first bytes in new header, so we will fill our header buffer with missing bytes from child
    // header and copy it to child header, because our node will get destroyed anyway. If child is a
    // leaf, there is nothing to do - we will just return it.
    //
    [[nodiscard]] entry_ptr collapse() noexcept
    {
        assert(m_num_children == 1);
        assert(m_children[0]);

        entry_ptr child_entry{m_children[0]};
        if (child_entry.node()) {
            Node* child_node = child_entry.node_ptr();

            uint32_t len = hdrlen(m_prefix_len);
            if (len < max_prefix_len)
                m_prefix[len++] = m_keys[0]; // Key for mapping.

            uint32_t i = 0;
            while (len < max_prefix_len && i < child_node->m_prefix_len)
                m_prefix[len++] = child_node->m_prefix[i++];

            std::memcpy(child_node->m_prefix, m_prefix, len);
            child_node->m_prefix_len += m_prefix_len + 1;
        }

        return child_entry;
    }

    [[nodiscard]] bool full() const noexcept { return m_num_children == 4; }

    [[nodiscard]] bool should_shrink() const noexcept { return false; };

    [[nodiscard]] Node16* grow() noexcept { return new Node16{*this}; }

    [[nodiscard]] const Leaf& next_leaf() const noexcept { return m_children[0].next_leaf(); }

    uint8_t m_keys[4]{};       // Keys with span of 1 byte.
    entry_ptr m_children[4]{}; // Pointers to children nodes.
};

// Node16: This node type is used for storing between 5 and 16 child pointers. Like the Node4,
// the keys and pointers are stored in separate arrays at corresponding positions, but both
// arrays have space for 16 entries. A key can be found efficiently with binary search or, on
// modern hardware, with parallel comparisons using SIMD instructions.
//
template<class T>
class Node16 final : public Node<T> {
public:
    using Node = Node<T>;
    using Node4 = Node4<T>;
    using Node48 = Node48<T>;
    using Node256 = Node256<T>;
    using Leaf = Leaf<T>;
    using entry_ptr = entry_ptr<T>;

    using Node::m_num_children;
    using Node::m_prefix;
    using Node::m_prefix_len;
    using Node::m_type;

    Node16(Node4& old_node) noexcept : Node{old_node, Node16_t}
    {
        std::memcpy(m_keys, old_node.m_keys, 4);
        std::memcpy(m_children, old_node.m_children, 4 * sizeof(entry_ptr));
    }

    Node16(Node48& old_node) noexcept : Node{old_node, Node16_t}
    {
        size_t idx = 0;
        for (size_t old_idx = 0; old_idx < 256; ++old_idx) {
            if (old_node.m_idxs[old_idx] != Node48::empty_slot) {
                m_keys[idx] = old_idx;
                m_children[idx] = old_node.m_children[old_node.m_idxs[old_idx]];

                ++idx;
            }
        }

        assert(idx == m_num_children);
    }

    // TODO: Check if compiler generates SIMD instructions. If not, insert them manually.
    //
    [[nodiscard]] entry_ptr* find_child(uint8_t key) noexcept
    {
        for (int i = 0; i < m_num_children; ++i)
            if (m_keys[i] == key)
                return &m_children[i];

        return nullptr;
    }

    // Adds new child to node.
    // Finds place for new child (while keeping sorted order) and moves all children by 1 place to
    // make space for new child. Inserts new child after that.
    //
    void add_child(const uint8_t key, entry_ptr child) noexcept
    {
        assert(m_num_children < 16);

        size_t idx = 0;
        while (idx < m_num_children && m_keys[idx] < key)
            ++idx;

        for (size_t i = m_num_children; i > idx; --i) {
            m_keys[i] = m_keys[i - 1];
            m_children[i] = m_children[i - 1];
        }

        m_keys[idx] = key;
        m_children[idx] = child;

        ++m_num_children;
    }

    void remove_child(const uint8_t key) noexcept
    {
        size_t idx = 0;
        while (m_keys[idx] != key)
            ++idx;

        assert(idx < m_num_children);
        m_children[idx].reset();

        for (; idx < m_num_children - 1; ++idx) {
            m_keys[idx] = m_keys[idx + 1];
            m_children[idx] = m_children[idx + 1];
        }

        m_keys[idx] = 0;
        m_children[idx] = nullptr;

        --m_num_children;
    }

    [[nodiscard]] bool full() const noexcept { return m_num_children == 16; }

    [[nodiscard]] bool should_shrink() const noexcept
    {
        return m_num_children == CFG_node16_shrink_threshold;
    };

    [[nodiscard]] Node48* grow() noexcept { return new Node48{*this}; }

    [[nodiscard]] Node4* shrink() noexcept { return new Node4{*this}; }

    [[nodiscard]] const Leaf& next_leaf() const noexcept { return m_children[0].next_leaf(); }

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
template<class T>
class Node48 final : public Node<T> {
public:
    using Node = Node<T>;
    using Node4 = Node4<T>;
    using Node16 = Node16<T>;
    using Node256 = Node256<T>;
    using Leaf = Leaf<T>;
    using entry_ptr = entry_ptr<T>;

    using Node::m_num_children;
    using Node::m_prefix;
    using Node::m_prefix_len;
    using Node::m_type;

    static constexpr uint8_t empty_slot = 255;

    Node48(Node16& old_node) noexcept : Node{old_node, Node48_t}
    {
        std::memset(m_idxs, empty_slot, 256);
        std::memcpy(m_children, old_node.m_children, 16 * sizeof(entry_ptr));

        for (size_t i = 0; i < 16; ++i)
            m_idxs[old_node.m_keys[i]] = i;
    }

    Node48(Node256& old_node) noexcept : Node{old_node, Node48_t}
    {
        std::memset(m_idxs, empty_slot, 256);

        size_t idx = 0;
        for (size_t old_idx = 0; old_idx < 256; ++old_idx) {
            if (old_node.m_children[old_idx]) {
                m_idxs[old_idx] = idx;
                m_children[idx] = old_node.m_children[old_idx];

                ++idx;
            }
        }

        assert(idx == m_num_children);
    }

    [[nodiscard]] entry_ptr* find_child(uint8_t key) noexcept
    {
        return m_idxs[key] != empty_slot ? &m_children[m_idxs[key]] : nullptr;
    }

    void add_child(const uint8_t key, entry_ptr child) noexcept
    {
        assert(m_num_children < 48);
        assert(m_idxs[key] == empty_slot);

        size_t idx = 0;
        while (m_children[idx])
            ++idx;

        m_idxs[key] = idx;
        m_children[idx] = child;

        ++m_num_children;
    }

    void remove_child(const uint8_t key) noexcept
    {
        assert(m_idxs[key] != empty_slot);

        size_t idx = m_idxs[key];
        m_idxs[key] = empty_slot;
        m_children[idx].reset();

        --m_num_children;
    }

    [[nodiscard]] bool full() const noexcept { return m_num_children == 48; }

    [[nodiscard]] bool should_shrink() const noexcept
    {
        return m_num_children == CFG_node48_shrink_threshold;
    };

    [[nodiscard]] Node256* grow() noexcept { return new Node256{*this}; }

    [[nodiscard]] Node16* shrink() noexcept { return new Node16{*this}; }

    [[nodiscard]] const Leaf& next_leaf() const noexcept
    {
        for (int i = 0; i < 256; ++i)
            if (m_idxs[i] != empty_slot)
                return m_children[m_idxs[i]].next_leaf();

        assert(false);
        return *Leaf::new_leaf(Key{nullptr, 0}, nullptr);
    }

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
template<class T>
class Node256 final : public Node<T> {
public:
    using Node = Node<T>;
    using Node4 = Node4<T>;
    using Node16 = Node16<T>;
    using Node48 = Node48<T>;
    using Leaf = Leaf<T>;
    using entry_ptr = entry_ptr<T>;

    using Node::m_num_children;
    using Node::m_prefix;
    using Node::m_prefix_len;
    using Node::m_type;

    Node256(const Node48& old_node) noexcept : Node{old_node, Node256_t}
    {
        for (size_t i = 0; i < 256; ++i)
            if (old_node.m_idxs[i] != Node48::empty_slot)
                m_children[i] = old_node.m_children[old_node.m_idxs[i]];
    }

    [[nodiscard]] entry_ptr* find_child(uint8_t key) noexcept
    {
        return m_children[key] ? &m_children[key] : nullptr;
    }

    void add_child(const uint8_t key, entry_ptr child) noexcept
    {
        assert(!m_children[key]);

        m_children[key] = child;
        ++m_num_children;
    }

    void remove_child(const uint8_t key) noexcept
    {
        assert(m_children[key]);

        m_children[key].reset();
        --m_num_children;
    }

    [[nodiscard]] bool full() const noexcept { return m_num_children == 256; }

    [[nodiscard]] bool should_shrink() const noexcept
    {
        return m_num_children == CFG_node256_shrink_threshold;
    };

    [[nodiscard]] Node48* shrink() noexcept { return new Node48{*this}; }

    [[nodiscard]] const Leaf& next_leaf() const noexcept
    {
        for (int i = 0; i < 256; ++i)
            if (m_children[i])
                return m_children[i].next_leaf();

        assert(false);
        return *Leaf::new_leaf(Key{nullptr, 0}, nullptr);
    }

    entry_ptr m_children[256]{}; // Pointers to children nodes.
};

template<class T>
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

public:
    void* m_value = nullptr;
    size_t m_key_len;
    uint8_t m_key[];
};

template<class T>
class ART final {
public:
    using Node = Node<T>;
    using Node4 = Node4<T>;
    using Node16 = Node16<T>;
    using Node48 = Node48<T>;
    using Node256 = Node256<T>;
    using Leaf = Leaf<T>;
    using entry_ptr = entry_ptr<T>;

    ~ART() noexcept { destroy_all(m_root); }

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

    // Returns size of whole tree in bytes.
    // By default, it include whole leafs. Leaf keys can be disabled by passing false.
    //
    size_t size_in_bytes(bool full_leaves) const noexcept
    {
        return sizeof(ART) + size_in_bytes(m_root, full_leaves);
    }

private:
    // Handles insertion when we reached leaf node. With current implementation, we will only insert
    // new key if it differs from an exitisting leaf.
    // TODO: Maybe implement substitution of the old entry.
    //
    void insert_at_leaf(entry_ptr& entry, const Key& key, void* value, size_t depth) noexcept
    {
        Leaf* leaf = entry.leaf_ptr();
        if (!leaf->match(key))
            entry = new Node4{key, value, depth, leaf};
    }

    // Handles insertion when we reached innder node and key at provided depth did not match full
    // node prefix.
    //
    void insert_at_node(entry_ptr& entry, const Key& key, void* value, size_t depth,
                        size_t cp_len) noexcept
    {
        Node* node = entry.node_ptr();
        assert(cp_len != node->m_prefix_len);

        entry = new Node4{key, value, depth, node, cp_len};
    }

    // Insert new key into the tree.
    //
    void insert(const Key& key, void* value) noexcept
    {
        if (m_root)
            return insert(m_root, key, value, 0);

        m_root = Leaf::new_leaf(key, value);
    }

    // Inserts new key into the tree.
    //
    void insert(entry_ptr& entry, const Key& key, void* value, size_t depth) noexcept
    {
        assert(entry);

        if (entry.leaf())
            return insert_at_leaf(entry, key, value, depth);

        Node* node = entry.node_ptr();
        size_t cp_len = node->common_prefix(key, depth);

        if (cp_len != node->m_prefix_len)
            return insert_at_node(entry, key, value, depth, cp_len);

        assert(cp_len == node->m_prefix_len);
        depth += node->m_prefix_len;

        entry_ptr* next = node->find_child(key[depth]);
        if (next)
            return insert(*next, key, value, depth + 1);

        if (node->full())
            entry.reset(node = node->grow());

        node->add_child(key[depth], Leaf::new_leaf(key, value));
    }

    void erase(const Key& key) noexcept
    {
        if (!m_root)
            return;

        if (m_root.node())
            return erase(m_root, key, 0);

        Leaf* leaf = m_root.leaf_ptr();
        if (!leaf->match(key))
            return;

        m_root.reset();
    }

    // Erases leaf node.
    // If remaining children are below some treshold we will shrink node to save space.
    // If we have only 1 child left in node, we will replace node with it's child (collapse it).
    //
    void erase_leaf(entry_ptr& entry, Leaf* leaf, const Key& key, size_t depth) noexcept
    {
        if (!leaf->match(key))
            return;

        Node* node = entry.node_ptr();
        node->remove_child(key[depth]);

        if (node->should_shrink())
            entry.reset(node = node->shrink());

        if (node->should_collapse())
            entry.reset(node->collapse());
    }

    // Delete: The implementation of deletion is symmetrical to
    // insertion.The leaf is removed from an inner node,
    // which is shrunk if necessary. If that node now has only one child,
    // it is replaced by its child and the compressed path is adjusted.
    //
    void erase(entry_ptr& entry, const Key& key, size_t depth) noexcept
    {
        Node* node = entry.node_ptr();
        size_t cp_len = node->common_header_prefix(key, depth);

        if (cp_len != hdrlen(node->m_prefix_len))
            return;

        depth += node->m_prefix_len;

        entry_ptr* next = node->find_child(key[depth]);
        if (!next)
            return;

        if (next->node())
            return erase(*next, key, depth + 1);

        erase_leaf(entry, next->leaf_ptr(), key, depth);
    }

    Leaf* search(const Key& key) noexcept
    {
        if (m_root)
            return search(m_root, key, 0);

        return nullptr;
    }

    Leaf* search(entry_ptr& entry, const Key& key, size_t depth) noexcept
    {
        if (entry.leaf()) {
            Leaf* leaf = entry.leaf_ptr();
            if (leaf->match(key))
                return leaf;

            return nullptr;
        }

        Node* node = entry.node_ptr();
        size_t cp_len = node->common_header_prefix(key, depth);

        if (cp_len != hdrlen(node->m_prefix_len))
            return nullptr;

        depth += node->m_prefix_len;

        entry_ptr* next = node->find_child(key[depth]);
        if (next)
            return search(*next, key, depth + 1);

        return nullptr;
    }

    // Recursively visits all entries in a subtree and deletes them.
    //
    void destroy_all(entry_ptr& entry)
    {
        if (!entry)
            return;

        if (entry.node()) {
            Node* node = entry.node_ptr();

            entry_ptr* children = nullptr;
            size_t children_len = 0;

            switch (node->m_type) { // clang-format off
            case Node4_t:   children_len = 4,   children = node->node4()->m_children;   break;
            case Node16_t:  children_len = 16,  children = node->node16()->m_children;  break;
            case Node48_t:  children_len = 48,  children = node->node48()->m_children;  break;
            case Node256_t: children_len = 256, children = node->node256()->m_children; break;
            default: assert(false);                                                     break;
            } // clang-format on

            for (size_t i = 0; i < children_len; ++i)
                destroy_all(children[i]);
        }

        entry.reset();
    }

    size_t size_in_bytes(const entry_ptr& entry, bool full_leaves) const noexcept
    {
        if (!entry)
            return 0;

        if (entry.leaf())
            return sizeof(Leaf) + (full_leaves ? entry.leaf_ptr()->m_key_len : 0);

        Node* node = entry.node_ptr();
        size_t size = 0;
        entry_ptr* children = nullptr;
        size_t children_len = 0;

        switch (node->m_type) { // clang-format off
        case Node4_t:   size += sizeof(Node4),   children_len = 4,   children = node->node4()->m_children;   break;
        case Node16_t:  size += sizeof(Node16),  children_len = 16,  children = node->node16()->m_children;  break;
        case Node48_t:  size += sizeof(Node48),  children_len = 48,  children = node->node48()->m_children;  break;
        case Node256_t: size += sizeof(Node256), children_len = 256, children = node->node256()->m_children; break;
        default: assert(false);                                                                              break;
        } // clang-format on

        for (size_t i = 0; i < children_len; ++i)
            size += size_in_bytes(children[i], full_leaves);

        return size;
    }

    bool empty() const noexcept { return m_root; }

private:
    entry_ptr m_root;
};

} // namespace art

// NOLINTEND

#endif // ART_H
