// Implementation of an adaptive suffix trie.
//
#ifndef AST_H
#define AST_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "ptr_tag.h"

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using sz = std::size_t;

// NOLINTBEGIN

namespace ast {

constexpr sz CFG_node16_shrink_threshold = 2;
constexpr sz CFG_node48_shrink_threshold = 14;
constexpr sz CFG_node256_shrink_threshold = 46;

enum Node_t : u8 {
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

template<class T>
class AST;

static constexpr u32 max_prefix_len = 9;

// Limits provided length with maximum header length and returns it.
//
template<typename Type>
u32 hdrlen(Type len)
{
    return std::min(static_cast<u32>(len), max_prefix_len);
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
class KeySpan {
public:
    template<class T>
    friend class Leaf;

    static constexpr u8 term_byte = 0;

    const u8* data() const noexcept { return m_data; }

    sz size() const noexcept { return m_size; }

    KeySpan(const u8* const data, sz size) noexcept : m_data{data}, m_size{size + 1} {}

    KeySpan(const std::string& s) noexcept : KeySpan{(const u8* const)s.data(), s.size()} {}

    u8 operator[](sz idx) const noexcept
    {
        assert(idx < m_size);
        return idx == m_size - 1 ? term_byte : m_data[idx];
    }

    bool operator==(const KeySpan& other) const noexcept
    {
        if (m_size != other.m_size)
            return false;

        return !std::memcmp(m_data, other.m_data, m_size);
    }

    bool match_prefix(const KeySpan& other) const noexcept
    {
        if (other.size() > m_size)
            return false;

        // FIXME! This should take into accout a terminal 0.
        return !std::memcmp(m_data, other.m_data, other.size());
    }

    // Copy key to destination buffer with size len. We must manually copy last 0 byte, since we
    // don't hold that value in our buffer. User must assure that buffer can hold at least len
    // bytes.
    //
    void copy_to(u8* dest, sz len) const noexcept
    {
        std::memcpy(dest, m_data, std::min(len, m_size - 1));

        if (len >= m_size)
            dest[m_size - 1] = term_byte;
    }

private:
    const u8* const m_data;
    sz m_size;
};

/**
 * Data node holding value, key size and key.
 */
template<class T>
class KeyValue {
public:
    static KeyValue* new_kv(const KeySpan& key)
    {
        KeyValue* kv = static_cast<KeyValue*>(std::malloc(sizeof(KeyValue) + key.size()));
        kv->m_key_size = key.size();
        key.copy_to(kv->m_key, kv->m_key_size);

        return kv;
    }

    constexpr bool empty() { return m_key_size == 0; }

    constexpr u32 size() { return m_key_size; }

    constexpr const u8* key() { return m_key; }

private:
    u32 m_key_size;
    u8 m_key[];
};

/**
 * Key reference structure used to access data key at the specific (suffix) offset. It should be the
 * same size as pointer, since it can be stored in an entry_ptr.
 * Since we are using pointer tags to distinct pointer to node, leaf and KeyRef, we will shift
 * offset by 2 places in order to make the room for the pointer tag.
 */
template<class T>
class KeyRef {
public:
    KeyRef() = default;

    KeyRef(u32 idx, u32 offset) : m_offset{offset}, m_idx{idx} {}

    constexpr u32 idx() const noexcept { return m_idx; }

    constexpr u32 offset() const noexcept { return m_offset; }

    constexpr KeyRef& next() noexcept
    {
        ++m_offset;
        return *this;
    }

    KeyRef& pack()
    {
        m_offset <<= tag_bits; // FIXME! Shift by tag bits (3, not 2)!
        return *this;
    }

    KeyRef& unpack()
    {
        m_offset >>= tag_bits;
        return *this;
    }

private:
    u32 m_offset; // Prefix offset from the start of the key.
    u32 m_idx;    // Data index in data array.
};

static_assert(sizeof(KeyRef<void>) == sizeof(void*));

/**
 * Leaf holding list of KeyRefs. Each reference is used to access original key at the specific
 * offset. This list act as vector: as soon as it is populated, we will double the storage and copy
 * an existing elements. Note that whole AST functionalities depends on elements beeing packed
 * without empty slots.
 */
template<class T>
class Leaf {
public:
    using KeyRef = KeyRef<T>;

    static Leaf* new_leaf(u32 size = 2)
    {
        Leaf* leaf = static_cast<Leaf*>(malloc(sizeof(Leaf) + sizeof(KeyRef) * size));
        leaf->m_size = 0;
        leaf->m_capacity = size;
        return leaf;
    }

    [[nodiscard]] Leaf* push_back(KeyRef ref)
    {
        Leaf* leaf = this;
        if (m_size == m_capacity) {
            leaf = static_cast<Leaf*>(malloc(sizeof(Leaf) + sizeof(KeyRef) * m_capacity * 2));
            leaf->m_size = m_size;
            leaf->m_capacity = m_capacity * 2;
            std::memcpy(leaf->m_refs, m_refs, m_capacity);
            free(this);
        }

        leaf->m_refs[leaf->m_size++] = ref;
        return leaf;
    }

    [[nodiscard]] KeyRef next_key_ref() { return m_refs[0]; }

    u32 m_size;
    u32 m_capacity;
    KeyRef m_refs[];
};

/**
 * Wrapper class that holds either pointer to node, a pointer to Leaf (list of links) or a pointer
 * the Data (single link pointer). It is not a smart pointer, hence user must manage
 * memory manually. In order to simplify assigning another pointer with freeing current resources,
 * reset() funtion is implemented.
 *
 * Note: link is just a storage optimization. In case that we have a single link pointer, we don't
 * need to allocate full leaf node, but we can embed address of the original data directly in this
 * pointer. In case that we have multiple links, we need to allocate leaf and store them there.
 */
template<class T>
class entry_ptr {
public:
    using Node = Node<T>;
    using Leaf = Leaf<T>;
    using KeyRef = KeyRef<T>;

private:
    static constexpr uintptr_t node_tag = 0;
    static constexpr uintptr_t leaf_tag = 1;
    static constexpr uintptr_t ref_tag = 2;
    static_assert(ref_tag <= tag_bits);

public:
    constexpr entry_ptr() noexcept : m_ptr{nullptr} {}

    constexpr entry_ptr(std::nullptr_t) noexcept : m_ptr{nullptr} {}

    constexpr entry_ptr(Node* node) noexcept : m_ptr{node} {}

    constexpr entry_ptr(const Node* node) noexcept : m_ptr{node} {}

    constexpr entry_ptr(Leaf* leaf) noexcept : m_ptr{set_tag(leaf, leaf_tag)} {}

    constexpr entry_ptr(const Leaf* leaf) noexcept : m_ptr{set_tag(leaf, leaf_tag)} {}

    constexpr entry_ptr(KeyRef ref) noexcept
        : m_ptr{set_tag(std::bit_cast<const void*>(ref.pack()), ref_tag)}
    {
    }

    constexpr operator bool() const noexcept { return m_ptr != nullptr; }

    [[nodiscard]] constexpr bool leaf() const noexcept { return tag(m_ptr) == leaf_tag; }

    [[nodiscard]] constexpr bool node() const noexcept { return tag(m_ptr) == node_tag; }

    [[nodiscard]] constexpr bool ref() const noexcept { return tag(m_ptr) == ref_tag; }

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

    [[nodiscard]] constexpr KeyRef key_ref() const noexcept
    {
        assert(ref());
        return std::bit_cast<KeyRef>(m_ptr).unpack();
    }

    /**
     * Returns next data node reference from the current entry.
     * If current entry is link, it returns it.
     * If it is node, it calls next_key_ref() on a node which will recurse down one level based on
     * node node type an call us again. If it is a leaf, it finds first non null data pointer and
     * returns it.
     */
    [[nodiscard]] constexpr KeyRef next_key_ref() const noexcept
    {
        assert(*this);

        if (ref())
            return key_ref();

        if (node())
            return node_ptr()->next_key_ref();

        return leaf_ptr()->next_key_ref();
    }

    /**
     * Frees memory based on pointer type and sets pointer to other (nullptr by default).
     */
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
            else if (leaf()) {
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
    using AST = AST<T>;
    using Node4 = Node4<T>;
    using Node16 = Node16<T>;
    using Node48 = Node48<T>;
    using Node256 = Node256<T>;
    using Leaf = Leaf<T>;
    using KeyRef = KeyRef<T>;
    using entry_ptr = entry_ptr<T>;

    // Construct new node from previous node by copying whole header except node type which is
    // provided as new_type.
    //
    Node(const Node& other, u8 new_type)
        : m_prefix_len{other.m_prefix_len}
        , m_num_children{other.m_num_children}
        , m_type{new_type}
    {
        if (other.m_prefix_len > 0)
            std::memcpy(m_prefix, other.m_prefix, hdrlen(other.m_prefix_len));
    }

    Node(u8 type) noexcept : m_type{type} {}

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

    [[nodiscard]] entry_ptr* find_child(u8 key) noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t:   return node4()->find_child(key);
        case Node16_t:  return node16()->find_child(key);
        case Node48_t:  return node48()->find_child(key);
        case Node256_t: return node256()->find_child(key);
        default:        return assert(!"Invalid case."), nullptr;
        } // clang-format on
    }

    void add_child(u8 key, entry_ptr child) noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t:   return node4()->add_child(key, child);
        case Node16_t:  return node16()->add_child(key, child);
        case Node48_t:  return node48()->add_child(key, child);
        case Node256_t: return node256()->add_child(key, child);
        default:        return assert(!"Invalid case.");
        } // clang-format on
    }

    void remove_child(u8 key) noexcept
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

    [[nodiscard]] KeyRef next_key_ref() const noexcept
    {
        switch (m_type) { // clang-format off
        case Node4_t:   return node4()->next_key_ref();
        case Node16_t:  return node16()->next_key_ref();
        case Node48_t:  return node48()->next_key_ref();
        case Node256_t: return node256()->next_key_ref();
        default:        return assert(!"Invalid case."), KeyRef{};
        } // clang-format on
    }

    // Returns common prefix len for key at current depth and a prefix in our header.
    // Header must not hold terminal byte, so we are comparing to minimal of node prefix len and max
    // prefix len. If depth reaches key.size() - 1, we will get terminal byte and stop comparing.
    //
    [[nodiscard]] sz common_header_prefix(const KeySpan& key, sz depth) const noexcept
    {
        const sz max_cmp = hdrlen(m_prefix_len);
        sz cp_len = 0;

        while (cp_len < max_cmp && key[depth + cp_len] == m_prefix[cp_len])
            ++cp_len;

        return cp_len;
    }

    // Returns common prefix length from this node to the leaf and a key at provided depth.
    // If prefix in our header is max_prefix_len long and we've matched whole prefix, we need to
    // find leaf to keep comparing. It is not important which leaf we take, because all of them have
    // at least m_prefix_len common bytes.
    //
    [[nodiscard]] sz common_prefix(const AST& ctx, const KeySpan& key, sz depth) const noexcept
    {
        sz cp_len = common_header_prefix(key, depth);

        if (cp_len == max_prefix_len && cp_len < m_prefix_len) {
            const KeySpan leaf_key = ctx.m_data[next_key_ref()];
            while (cp_len < m_prefix_len && key[depth + cp_len] == leaf_key[depth + cp_len])
                ++cp_len;
        }

        return cp_len;
    }

    u32 m_prefix_len = 0;
    u16 m_num_children = 0;
    u8 m_type;
    u8 m_prefix[max_prefix_len];
};

static_assert(sizeof(Node<void>) == 16);

// Node4: The smallest node type can store up to 4 child pointers and uses an array of length 4
// for keys and another array of the same length for pointers. The keys and pointers are stored
// at corresponding positions and the keys are sorted.
//
template<class T>
class Node4 final : public Node<T> {
public:
    using AST = AST<T>;
    using Node = Node<T>;
    using Node16 = Node16<T>;
    using Node48 = Node48<T>;
    using Node256 = Node256<T>;
    using Leaf = Leaf<T>;
    using KeyRef = KeyRef<T>;
    using KeyValue = KeyValue<T>;
    using entry_ptr = entry_ptr<T>;

    using Node::m_num_children;
    using Node::m_prefix;
    using Node::m_prefix_len;
    using Node::m_type;

    Node4(const KeySpan& key, KeyRef value, sz depth, const KeySpan& old_key, KeyRef old_value)
        : Node{Node4_t}
    {
        for (m_prefix_len = 0; key[depth] == old_key[depth]; ++m_prefix_len, ++depth)
            if (m_prefix_len < max_prefix_len)
                m_prefix[m_prefix_len] = key[depth];

        assert(depth < key.size() && depth < old_key.size());
        assert(key[depth] != old_key[depth]);

        add_child(key[depth], value);
        add_child(old_key[depth], old_value);
    }

    // Creates new node4 as a parent for new_leaf and old leaf. New node will have 2
    // children with common prefix extracted from them. We will store information about common
    // prefix len in prefix_len, but we can only copy up to max_prefix_bytes in prefix array.
    // Later when searching for a key, we will compare all bytes from our prefix array, and if
    // all bytes matches, we will just skip all prefix bytes and go to directly to next node.
    // This is hybrid approach which mixes optimistic and pessimistic approaches.
    //
    Node4(const KeySpan& key, KeyRef value, sz depth, KeySpan old_key, const Leaf& old_leaf)
        : Node{Node4_t}
    {
        for (m_prefix_len = 0; key[depth] == old_key[depth]; ++m_prefix_len, ++depth)
            if (m_prefix_len < max_prefix_len)
                m_prefix[m_prefix_len] = key[depth];

        assert(depth < key.size() && depth < old_key.size());
        assert(key[depth] != old_key[depth]);

        add_child(key[depth], value);
        add_child(old_key[depth], &old_leaf);
    }

    Node4(const AST& ctx, const KeySpan& key, KeyRef value, sz depth, Node& node, sz cp_len)
        : Node{Node4_t}
    {
        assert(cp_len < node.m_prefix_len);

        m_prefix_len = cp_len;
        std::memcpy(m_prefix, node.m_prefix, hdrlen(cp_len));

        const u8* prefix_src = node.m_prefix_len <= max_prefix_len ?
                                   &node.m_prefix[cp_len] :
                                   ctx.m_data[node.next_key_ref()].data() + depth + cp_len;

        // FIXME! I am not sure whether above data access within keyspan will work. Check it.

        const u8 node_key = prefix_src[0]; // Byte for node mapping.

        node.m_prefix_len -= (cp_len + 1); // Additional byte for node mapping.
        std::memmove(node.m_prefix, prefix_src + 1, hdrlen(node.m_prefix_len));

        assert(node_key != key[depth + cp_len]);

        add_child(node_key, &node);
        add_child(key[depth + cp_len], value);
    }

    Node4(const Node16& old_node) noexcept : Node{old_node, Node4_t}
    {
        std::memcpy(m_keys, old_node.m_keys, old_node.m_num_children);
        std::memcpy(m_children, old_node.m_children, old_node.m_num_children * sizeof(entry_ptr));
    }

    [[nodiscard]] entry_ptr* find_child(u8 key) noexcept
    {
        for (u16 i = 0; i < m_num_children; ++i)
            if (m_keys[i] == key)
                return &m_children[i];

        return nullptr;
    }

    // Adds new child to node.
    // Finds place for new child (while keeping sorted order) and moves all children by 1 place to
    // make space for new child. Inserts new child after that.
    //
    void add_child(u8 key, entry_ptr child) noexcept
    {
        assert(m_num_children < 4);

        u16 idx = 0;
        while (idx < m_num_children && m_keys[idx] < key)
            ++idx;

        for (u16 i = m_num_children; i > idx; --i) {
            m_keys[i] = m_keys[i - 1];
            m_children[i] = m_children[i - 1];
        }

        m_keys[idx] = key;
        m_children[idx] = child;

        ++m_num_children;
    }

    void remove_child(u8 key) noexcept
    {
        u16 idx = 0;
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

            u32 len = hdrlen(m_prefix_len);
            if (len < max_prefix_len)
                m_prefix[len++] = m_keys[0]; // Key for mapping.

            u32 i = 0;
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

    [[nodiscard]] KeyRef next_key_ref() const noexcept { return m_children[0].next_key_ref(); }

    u8 m_keys[4]{};            // Keys with span of 1 byte.
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
    using KeyRef = KeyRef<T>;
    using KeyValue = KeyValue<T>;
    using entry_ptr = entry_ptr<T>;

    using Node::m_num_children;
    using Node::m_prefix;
    using Node::m_prefix_len;
    using Node::m_type;

    Node16(const Node4& old_node) noexcept : Node{old_node, Node16_t}
    {
        std::memcpy(m_keys, old_node.m_keys, 4);
        std::memcpy(m_children, old_node.m_children, 4 * sizeof(entry_ptr));
    }

    Node16(const Node48& old_node) noexcept : Node{old_node, Node16_t}
    {
        u16 idx = 0;
        for (u16 old_idx = 0; old_idx < 256; ++old_idx) {
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
    [[nodiscard]] entry_ptr* find_child(u8 key) noexcept
    {
        for (u16 i = 0; i < m_num_children; ++i)
            if (m_keys[i] == key)
                return &m_children[i];

        return nullptr;
    }

    // Adds new child to node.
    // Finds place for new child (while keeping sorted order) and moves all children by 1 place to
    // make space for new child. Inserts new child after that.
    //
    void add_child(u8 key, entry_ptr child) noexcept
    {
        assert(m_num_children < 16);

        u16 idx = 0;
        while (idx < m_num_children && m_keys[idx] < key)
            ++idx;

        for (u16 i = m_num_children; i > idx; --i) {
            m_keys[i] = m_keys[i - 1];
            m_children[i] = m_children[i - 1];
        }

        m_keys[idx] = key;
        m_children[idx] = child;

        ++m_num_children;
    }

    void remove_child(u8 key) noexcept
    {
        u16 idx = 0;
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

    [[nodiscard]] KeyRef next_key_ref() const noexcept { return m_children[0].next_key_ref(); }

    u8 m_keys[16]{};            // Keys with span of 1 byte.
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
    using KeyRef = KeyRef<T>;
    using KeyValue = KeyValue<T>;
    using entry_ptr = entry_ptr<T>;

    using Node::m_num_children;
    using Node::m_prefix;
    using Node::m_prefix_len;
    using Node::m_type;

    static constexpr u8 empty_slot = 255;

    Node48(const Node16& old_node) noexcept : Node{old_node, Node48_t}
    {
        std::memset(m_idxs, empty_slot, 256);
        std::memcpy(m_children, old_node.m_children, 16 * sizeof(entry_ptr));

        for (u16 i = 0; i < 16; ++i)
            m_idxs[old_node.m_keys[i]] = i;
    }

    Node48(const Node256& old_node) noexcept : Node{old_node, Node48_t}
    {
        std::memset(m_idxs, empty_slot, 256);

        u16 idx = 0;
        for (u16 old_idx = 0; old_idx < 256; ++old_idx) {
            if (old_node.m_children[old_idx]) {
                m_idxs[old_idx] = idx;
                m_children[idx] = old_node.m_children[old_idx];

                ++idx;
            }
        }

        assert(idx == m_num_children);
    }

    [[nodiscard]] entry_ptr* find_child(u8 key) noexcept
    {
        return m_idxs[key] != empty_slot ? &m_children[m_idxs[key]] : nullptr;
    }

    void add_child(u8 key, entry_ptr child) noexcept
    {
        assert(m_num_children < 48);
        assert(m_idxs[key] == empty_slot);

        u16 idx = 0;
        while (m_children[idx])
            ++idx;

        m_idxs[key] = idx;
        m_children[idx] = child;

        ++m_num_children;
    }

    void remove_child(u8 key) noexcept
    {
        assert(m_idxs[key] != empty_slot);

        u16 idx = m_idxs[key];
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

    [[nodiscard]] KeyRef next_key_ref() const noexcept
    {
        for (u16 i = 0; i < 256U; ++i)
            if (m_idxs[i] != empty_slot)
                return m_children[m_idxs[i]].next_key_ref();

        assert(false);
        return KeyRef{};
    }

    // Keys with span of 1 byte with value of index for m_idxs array.
    u8 m_idxs[256];
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
    using KeyRef = KeyRef<T>;
    using KeyValue = KeyValue<T>;
    using entry_ptr = entry_ptr<T>;

    using Node::m_num_children;
    using Node::m_prefix;
    using Node::m_prefix_len;
    using Node::m_type;

    Node256(const Node48& old_node) noexcept : Node{old_node, Node256_t}
    {
        for (u16 i = 0; i < 256; ++i)
            if (old_node.m_idxs[i] != Node48::empty_slot)
                m_children[i] = old_node.m_children[old_node.m_idxs[i]];
    }

    [[nodiscard]] entry_ptr* find_child(u8 key) noexcept
    {
        return m_children[key] ? &m_children[key] : nullptr;
    }

    void add_child(u8 key, entry_ptr child) noexcept
    {
        assert(!m_children[key]);

        m_children[key] = child;
        ++m_num_children;
    }

    void remove_child(u8 key) noexcept
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

    [[nodiscard]] KeyRef next_key_ref() const noexcept
    {
        for (u16 i = 0; i < 256; ++i)
            if (m_children[i])
                return m_children[i].next_key_ref();

        assert(false);
        return KeyRef{};
    }

    entry_ptr m_children[256]{}; // Pointers to children nodes.
};

// template<class T>
// class Leaf final {
// private:
// // Private constructor that initializes leaf. It constructs Leaf object in place on a allocated
// // memory from new_leaf().
// //
// template<class V>
// Leaf(const Key& key, V&& value) : m_value{std::forward<V>(value)}
// , m_key_size{key.size()}
// {
// key.copy_to(m_key, key.size());
// }

// public:
// // Creates new leaf from provided key and value. It allocates memory big enough to fit value,
// // key len and variable size key after which calls constructor on that memory.
// //
// template<class V>
// static Leaf* new_leaf(const Key& key, V&& value)
// {
// return new (std::malloc(sizeof(Leaf) + key.size())) Leaf{key, std::forward<V>(value)};
// }

// const u8& operator[](sz idx) const noexcept
// {
// assert(idx < m_key_size);
// return m_key[idx];
// }

// sz key_size() const noexcept { return m_key_size; }

// // Returns whether internal key and provided key matches. Since internal key already holds
// // terminal byte at the end and key doesn't, we must memcmp all except last byte.
// //
// bool match(const Key& key) const noexcept
// {
// if (m_key_size != key.size())
// return false;

// return !std::memcmp(m_key, key.m_data, m_key_size - 1);
// }

// // Matches provided key as a prefix for this leaf. We need to match all characters from key
// // except last terminal byte.
// //
// bool match_prefix(const Key& key) const noexcept
// {
// const sz cmp_size = key.size() - 1;

// if (cmp_size > m_key_size)
// return false;

// return !std::memcmp(m_key, key.m_data, std::min(m_key_size, cmp_size));
// }

// const T& value() const noexcept { return m_value; }

// T& value() noexcept { return m_value; }

// std::string key_to_string() const noexcept
// {
// return std::string(m_key, m_key + m_key_size - 1);
// }

// std::string_view key_to_string_view() const noexcept
// {
// return std::string_view{(const char*)m_key, m_key_size - 1};
// }

// private:
// T m_value;
// sz m_key_size;
// u8 m_key[];
// };

/**
 * Container (dynamic array) of KeyValues.
 * It is used to store KeyValue objects and to access specific key at provided offset. This is
 * neccesarry for suffix checks, because a lot of operations only check partial (suffix) key.
 */
template<class T>
class Data {
public:
    using KeyRef = KeyRef<T>;
    using KeyValue = KeyValue<T>;

    static constexpr sz init_size = 128;

    Data() : m_capacity{0}, m_kv{nullptr} {}

    Data(u32 size)
    {
        m_kv = calloc(size, sizeof(KeyValue*));
        m_capacity = size;
    }

    ~Data()
    {
        for (u32 i = 0; i < m_capacity; ++i)
            if (m_kv[i])
                free(m_kv[i]);
    }

    KeyRef insert(const KeySpan& key)
    {
        if (m_kv == nullptr) {
            m_kv = static_cast<KeyValue**>(calloc(init_size, sizeof(KeyValue*)));
            m_capacity = init_size;
        }

        for (u32 i = 0; i < m_capacity; ++i) {
            if (m_kv[i] == nullptr) {
                m_kv[i] = KeyValue::new_kv(key);
                return KeyRef{i, 0};
            }
        }

        KeyValue** old = m_kv;
        m_kv = static_cast<KeyValue**>(calloc(m_capacity * 2, sizeof(KeyValue*)));
        std::memcpy(m_kv, old, m_capacity * sizeof(KeyValue));
        free(old);

        m_kv[m_capacity] = KeyValue::new_kv(key);
        KeyRef ref{m_capacity, 0};
        m_capacity *= 2;

        return ref;
    }

    /**
     * Returns a key which represents specific key at offset -> suffix of a key.
     */
    KeySpan operator[](const KeyRef& ref) const
    {
        assert(ref.idx() < m_capacity);

        KeyValue* kv = m_kv[ref.idx()];
        assert(kv != nullptr);
        assert(kv->size() > ref.offset());

        return KeySpan{kv->key() + ref.offset(), kv->size() - 1 - ref.offset()};
    }

    KeyValue* operator[](u32 idx) const
    {
        assert(idx < m_capacity);
        assert(m_kv[idx] != nullptr); // FIXME! This is left for testing only.

        return m_kv[idx];
    }

private:
    u32 m_capacity;
    KeyValue** m_kv;
};

template<class T = void*>
class AST {
public:
    using Node = Node<T>;
    using Node4 = Node4<T>;
    using Node16 = Node16<T>;
    using Node48 = Node48<T>;
    using Node256 = Node256<T>;
    using Leaf = Leaf<T>;
    using Data = Data<T>;
    using KeyRef = KeyRef<T>;
    using entry_ptr = entry_ptr<T>;
    using KeyValue = KeyValue<T>;

    // Class that wraps insert result.
    // It holds a pointer to Leaf and a bool flag representing whether insert succeeded (read
    // insert for more details).
    //
    class result {
    public:
        result(Leaf* value, bool ok) : m_value{value}, m_ok{ok} { assert(m_value != nullptr); }

        Leaf* get() noexcept { return m_value; }

        const Leaf* get() const noexcept { return m_value; }

        constexpr bool ok() const noexcept { return m_ok; }

        Leaf* operator->() noexcept { return get(); }

        const Leaf* operator->() const noexcept { return get(); }

        constexpr operator bool() const noexcept { return ok(); }

    private:
        Leaf* m_value;
        bool m_ok;
    };

    AST() = default;

    ~AST() noexcept
    {
        for_each<visit_order::post_order>([](entry_ptr& entry) { entry.reset(); });
    }

    AST(const AST& other) = delete;
    AST(AST&& other) noexcept = delete;

    AST& operator=(const AST& other) = delete;
    AST& operator=(AST&& other) noexcept = delete;

    // Inserts single key/value pair into the tree. In order to support keys insertions without
    // values, we will default class T = void*, and default initialize value parameter. Also, in
    // order to avoid duplicating code, we need to introduce new template class V in order to
    // perfectly forward value to leaf. If we were to use T&& value as a parameter without
    // template declaration, compiler would treat it as a rvalue reference only.
    //
    void insert(const std::string& s) noexcept
    {
        const KeySpan key{s};

        KeyRef ref = m_data.insert(key);
        for (sz i = 0; i < key.size(); ++i) {
            KeySpan suffix{key.data() + i, key.size() - 1 - i};
            insert(suffix, ref);
            ref.next();
        }
    }

    // Inserts single key/value pair into the tree. In order to support keys insertions without
    // values, we will default class T = void*, and default initialize value parameter. Also, in
    // order to avoid duplicating code, we need to introduce new template class V in order to
    // perfectly forward value to leaf. If we were to use T&& value as a parameter without
    // template declaration, compiler would treat it as a rvalue reference only.
    //
    template<class V = T>
    result insert(const u8* const data, sz size, V&& value = V{}) noexcept
    {
        const KeySpan key{data, size};
        return insert(key, std::forward<V>(value));
    }

    void erase(const std::string& s) noexcept
    {
        const KeySpan key{(const u8* const)s.data(), s.size()};
        erase(key);
    }

    void erase(const u8* const data, sz size) noexcept
    {
        const KeySpan key{data, size};
        erase(key);
    }

    [[nodiscard]] KeyValue* const search(const std::string& s) const noexcept
    {
        const KeySpan key{(const u8* const)s.data(), s.size()};
        return search(key);
    }

    [[nodiscard]] KeyValue* const search(const u8* const data, sz size) const noexcept
    {
        const KeySpan key{data, size};
        return search(key);
    }

    // Finds all entries whose key starts with provided string. User can limit number of
    // returned entries with limit parameter.
    //
    [[nodiscard]] const std::vector<KeyValue*>
    search_prefix(const std::string& prefix,
                  sz limit = std::numeric_limits<sz>::max()) const noexcept
    {
        std::vector<KeyValue*> result;
        const KeySpan key{(const u8* const)prefix.data(), prefix.size()};

        search_prefix(key, result, limit);
        return result;
    }

    // Finds all entries whose key starts with provided prefix with given size. User can limit
    // number of returned entries with limit parameter.
    //
    [[nodiscard]] const std::vector<KeyValue*>
    search_prefix(const u8* const prefix, sz prefix_size,
                  sz limit = std::numeric_limits<sz>::max()) const noexcept
    {
        std::vector<const KeyValue*> result;
        const KeySpan key{prefix, prefix_size};

        search_prefix(key, result, limit);
        return result;
    }

    // Find an entry pointer whose key starts with provided string
    //
    [[nodiscard]] const entry_ptr search_prefix_node(const std::string& prefix) const noexcept
    {
        const KeySpan key{(const u8* const)prefix.data(), prefix.size()};
        return search_prefix_node(key);
    }

    // Find an entry pointer whose key starts with provided string.
    //
    [[nodiscard]] const entry_ptr search_prefix_node(const u8* const prefix,
                                                     sz prefix_size) const noexcept
    {
        const KeySpan key{prefix, prefix_size};
        return search_prefix_node(key);
    }

    // Finds all entries whose key starts with provided string. User can limit number of
    // returned entries with limit parameter.
    //
    template<class Pred>
    [[nodiscard]] const std::vector<Leaf*>
    search_prefix_if(const std::string& prefix, Pred pred,
                     sz limit = std::numeric_limits<sz>::max()) const noexcept
    {
        return search_prefix_if((const u8* const)prefix.data(), prefix.size(), pred, limit);
    }

    // Finds all entries whose key starts with provided prefix with given size. User can limit
    // number of returned entries with limit parameter.
    //
    template<class Pred>
    [[nodiscard]] const std::vector<Leaf*>
    search_prefix_if(const u8* const prefix, sz prefix_size, Pred pred,
                     sz limit = std::numeric_limits<sz>::max()) const noexcept
    {
        std::vector<Leaf*> leaves;
        const KeySpan key{prefix, prefix_size};

        search_prefix_if(key, leaves, pred, limit);
        return leaves;
    }

    sz nodes_count() const noexcept
    {
        sz c = 0;
        for_each_node([&](const Node* node) { ++c; });

        return c;
    }

    sz leaves_count() const noexcept
    {
        sz c = 0;
        for_each_leaf([&](const Leaf* leaf) { ++c; });

        return c;
    }

    sz nodes_size_in_bytes()
    {
        sz size = 0;
        AST::for_each_node([&](const Node* node) {
            switch (node->m_type) { // clang-format off
            case ast::Node4_t:   size += sizeof(Node4);   break;
            case ast::Node16_t:  size += sizeof(Node16);  break;
            case ast::Node48_t:  size += sizeof(Node48);  break;
            case ast::Node256_t: size += sizeof(Node256); break;
            default: assert(false);                       break;
            } // clang-format on
        });

        return size;
    }

    sz leaves_size_in_bytes()
    {
        sz size = 0;
        AST::for_each_leaf([&](const Leaf* leaf) { size += sizeof(Leaf) + leaf->m_capacity; });

        return size;
    }

    // Returns size of whole tree in bytes.
    // By default, it include whole leafs. Leaf keys can be disabled by passing false.
    //
    sz size_in_bytes() const noexcept
    {
        sz size = sizeof(AST);

        for_each([&](const entry_ptr& entry) {
            if (entry.leaf())
                size += sizeof(Leaf) + entry.leaf_ptr()->m_capacity();
            else {
                Node* node = entry.node_ptr();
                switch (node->m_type) { // clang-format off
                case Node4_t:   size += sizeof(Node4);   break;
                case Node16_t:  size += sizeof(Node16);  break;
                case Node48_t:  size += sizeof(Node48);  break;
                case Node256_t: size += sizeof(Node256); break;
                default: assert(false);                  break;
                } // clang-format on
            }
        });

        return size;
    }

    void print_stats()
    {
        std::cout << "-------------------------------\n";
        std::cout << "Trie size in bytes:   " << size_in_bytes() << "\n";
        std::cout << "Nodes size in bytes:  " << nodes_size_in_bytes() << "\n";
        std::cout << "Leaves size in bytes: " << leaves_size_in_bytes() << "\n";
        std::cout << "Nodes count:          " << nodes_count() << "\n";
        std::cout << "Leaves count:         " << leaves_count() << "\n";
        std::cout << "-------------------------------\n";
    }

    // ********************************************************************************************
    // Iterators section.
    // ********************************************************************************************

    // Order and type of iterations.
    //
    enum class visit_order { pre_order, post_order };
    enum class visit_type { node = 1, leaf = 2, any = node | leaf };

    // Don't look at this.
    //
    template<visit_type type>
    static constexpr bool noexcept_check(auto&& entry, const auto& callback)
    {
        if constexpr (type == visit_type::any)
            return noexcept(callback(entry));

        if constexpr (type == visit_type::node)
            return noexcept(callback(entry.node_ptr()));

        if constexpr (type == visit_type::leaf)
            return noexcept(callback(entry.leaf_ptr()));
    }

    // Default for each to root node.
    //
    template<visit_order order = visit_order::pre_order, visit_type type = visit_type::any>
    void for_each(const auto& callback) noexcept(noexcept(noexcept_check<type>(m_root, callback)))
    {
        for_each<order, type>(m_root, callback);
    }

    // Const version of for_each.
    //
    template<visit_order order = visit_order::pre_order, visit_type type = visit_type::any>
    void for_each(const auto& callback) const
        noexcept(noexcept(noexcept_check<type>(m_root, callback)))
    {
        for_each<order, type>(m_root, callback);
    }

    bool invoke_internal(auto&& entry, const auto& callback) noexcept(noexcept(callback(entry)))
    {
        if constexpr (std::is_void_v<decltype(callback(entry))>) {
            callback(entry);
            return true;
        }
        else {
            return callback(entry);
        }
    }

    bool invoke_internal(auto&& entry, const auto& callback) const
        noexcept(noexcept(callback(entry)))
    {
        if constexpr (std::is_void_v<decltype(callback(entry))>) {
            callback(entry);
            return true;
        }
        else {
            return callback(entry);
        }
    }

    // Invoked callable function and returns true if callback is void function, otherwise
    // returns result of callback. This is done in order to be able to break iterations in
    // for_each loop while allowing user to pass both void and bool functions. This is done for
    // fun and it is not necessary at all.
    //
    template<visit_type type>
    bool invoke(entry_ptr& entry,
                const auto& callback) noexcept(noexcept(noexcept_check<type>(entry, callback)))
    {
        if constexpr (type == visit_type::any)
            return invoke_internal(entry, callback);

        if constexpr (type == visit_type::node) {
            if (entry.node())
                return invoke_internal(entry.node_ptr(), callback);
        }

        if constexpr (type == visit_type::leaf) {
            if (entry.leaf())
                return invoke_internal(entry.leaf_ptr(), callback);
        }

        return true;
    }

    template<visit_type type>
    bool invoke(const entry_ptr& entry, const auto& callback) const
        noexcept(noexcept(noexcept_check<type>(entry, callback)))
    {
        if constexpr (type == visit_type::any)
            return invoke_internal(entry, callback);

        if constexpr (type == visit_type::node) {
            if (entry.node())
                return invoke_internal(entry.node_ptr(), callback);
        }

        if constexpr (type == visit_type::leaf) {
            if (entry.leaf())
                return invoke_internal(entry.leaf_ptr(), callback);
        }

        return true;
    }

    // Visits all entries and invokes callback on them.
    //
    template<visit_order order = visit_order::pre_order, visit_type type = visit_type::any>
    bool for_each(entry_ptr& entry,
                  const auto& callback) noexcept(noexcept(noexcept_check<type>(entry, callback)))
    {
        if (!entry)
            return true;

        if constexpr (order == visit_order::pre_order) {
            if (!invoke<type>(entry, callback))
                return false;
        }

        if (entry.node()) {
            Node* node = entry.node_ptr();

            entry_ptr* children = nullptr;
            sz children_len = 0;

            switch (node->m_type) { // clang-format off
            case Node4_t:   children_len = 4,   children = node->node4()->m_children;   break;
            case Node16_t:  children_len = 16,  children = node->node16()->m_children;  break;
            case Node48_t:  children_len = 48,  children = node->node48()->m_children;  break;
            case Node256_t: children_len = 256, children = node->node256()->m_children; break;
            default: assert(false);                                                     break;
            } // clang-format on

            for (sz i = 0; i < children_len; ++i)
                if (!for_each<order, type>(children[i], callback))
                    return false;
        }

        if constexpr (order == visit_order::post_order) {
            if (!invoke<type>(entry, callback))
                return false;
        }

        return true;
    }

    // Const for_each version.
    //
    template<visit_order order = visit_order::pre_order, visit_type type = visit_type::any>
    bool for_each(const entry_ptr& entry, const auto& callback) const
        noexcept(noexcept(noexcept_check<type>(entry, callback)))
    {
        if (!entry)
            return true;

        if constexpr (order == visit_order::pre_order) {
            if (!invoke<type>(entry, callback))
                return false;
        }

        if (entry.node()) {
            Node* node = entry.node_ptr();

            entry_ptr* children = nullptr;
            sz children_len = 0;

            switch (node->m_type) { // clang-format off
            case Node4_t:   children_len = 4,   children = node->node4()->m_children;   break;
            case Node16_t:  children_len = 16,  children = node->node16()->m_children;  break;
            case Node48_t:  children_len = 48,  children = node->node48()->m_children;  break;
            case Node256_t: children_len = 256, children = node->node256()->m_children; break;
            default: assert(false);                                                     break;
            } // clang-format on

            for (sz i = 0; i < children_len; ++i)
                if (!for_each<order, type>(children[i], callback))
                    return false;
        }

        if constexpr (order == visit_order::post_order) {
            if (!invoke<type>(entry, callback))
                return false;
        }

        return true;
    }

    template<visit_order order = visit_order::pre_order>
    void for_each_node(const auto& callback) noexcept(noexcept(callback(m_root.node_ptr())))
    {
        for_each<order, visit_type::node>(m_root, callback);
    }

    template<visit_order order = visit_order::pre_order>
    void for_each_node(const auto& callback) const noexcept(noexcept(callback(m_root.node_ptr())))
    {
        for_each<order, visit_type::node>(m_root, callback);
    }

    template<visit_order order = visit_order::pre_order>
    void for_each_node(entry_ptr& entry,
                       const auto& callback) noexcept(noexcept(callback(entry.node_ptr())))
    {
        for_each<order, visit_type::node>(entry, callback);
    }

    template<visit_order order = visit_order::pre_order>
    void for_each_node(const entry_ptr& entry, const auto& callback) const
        noexcept(noexcept(callback(entry.node_ptr())))
    {
        for_each<order, visit_type::node>(entry, callback);
    }

    template<visit_order order = visit_order::pre_order>
    void for_each_leaf(const auto& callback) noexcept(noexcept(callback(m_root.leaf_ptr())))
    {
        for_each<order, visit_type::leaf>(m_root, callback);
    }

    template<visit_order order = visit_order::pre_order>
    void for_each_leaf(const auto& callback) const noexcept(noexcept(callback(m_root.leaf_ptr())))
    {
        for_each<order, visit_type::leaf>(m_root, callback);
    }

    template<visit_order order = visit_order::pre_order>
    void for_each_leaf(entry_ptr& entry,
                       const auto& callback) noexcept(noexcept(callback(entry.leaf_ptr())))
    {
        for_each<order, visit_type::leaf>(entry, callback);
    }

    template<visit_order order = visit_order::pre_order>
    void for_each_leaf(const entry_ptr& entry, const auto& callback) const
        noexcept(noexcept(callback(entry.leaf_ptr())))
    {
        for_each<order, visit_type::leaf>(entry, callback);
    }

private:
    // Handles insertion when we reached entry and entry is a key reference.
    //
    void insert_at_entry(entry_ptr& entry, const KeySpan& key, KeyRef value, sz depth) noexcept
    {
        KeyRef old_ref = entry.key_ref();
        KeySpan old_key = m_data[old_ref];

        if (key == old_key) {
            Leaf* leaf = Leaf::new_leaf(2);
            leaf = leaf->push_back(old_ref);
            leaf = leaf->push_back(value);

            entry = leaf;
            return;
        }

        entry = new Node4{key, value, depth, old_key, old_ref};
    }

    /**
     * Handles insertion when we reached leaf node. With current implementation, we will only
     * insert new key if it differs from an exitisting leaf.
     */
    void insert_at_leaf(entry_ptr& entry, const KeySpan& key, KeyRef value, sz depth) noexcept
    {
        Leaf* leaf = entry.leaf_ptr();
        KeySpan old_key = m_data[leaf->next_key_ref()];

        if (key == old_key) {
            entry = leaf->push_back(value);
            return;
        }

        entry = new Node4{key, value, depth, old_key, *leaf};
    }

    // Handles insertion when we reached innder node and key at provided depth did not match
    // full node prefix.
    //
    void insert_at_node(entry_ptr& entry, const KeySpan& key, KeyRef value, sz depth,
                        sz cp_len) noexcept
    {
        Node* node = entry.node_ptr();
        assert(cp_len != node->m_prefix_len);

        entry = new Node4{*this, key, value, depth, *node, cp_len};
    }

    // Insert new key into the tree.
    //
    void insert(const KeySpan& key, KeyRef value) noexcept
    {
        if (m_root)
            return insert(m_root, key, value, 0);

        m_root = value;
    }

    // Inserts new key into the tree.
    //
    void insert(entry_ptr& entry, const KeySpan& key, KeyRef value, sz depth) noexcept
    {
        assert(entry);

        if (entry.ref())
            return insert_at_entry(entry, key, value, depth);

        if (entry.leaf())
            return insert_at_leaf(entry, key, value, depth);

        Node* node = entry.node_ptr();
        sz cp_len = node->common_prefix(*this, key, depth);

        if (cp_len != node->m_prefix_len)
            return insert_at_node(entry, key, value, depth, cp_len);

        assert(cp_len == node->m_prefix_len);
        depth += node->m_prefix_len;

        entry_ptr* next = node->find_child(key[depth]);
        if (next)
            return insert(*next, key, value, depth + 1);

        if (node->full())
            entry.reset(node = node->grow());

        node->add_child(key[depth], value);
    }

    void erase(const KeySpan& key) noexcept
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
    void erase_leaf(entry_ptr& entry, Leaf* leaf, const KeySpan& key, sz depth) noexcept
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
    void erase(entry_ptr& entry, const KeySpan& key, sz depth) noexcept
    {
        Node* node = entry.node_ptr();
        sz cp_len = node->common_header_prefix(key, depth);

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

    /**
     * Searches original KeyValue based on key (not suffix) in a trie and returns a pointer to it if
     * exists.
     * Note that this search complexity might deviate into linear, because it got to go
     * through all key refs in leaf node.
     */
    KeyValue* const search(const KeySpan& key) const noexcept
    {
        if (m_root)
            return search(m_root, key, 0);

        return nullptr;
    }

    KeyValue* const search(const entry_ptr& entry, const KeySpan& key, sz depth) const noexcept
    {
        if (entry.ref()) {
            KeyRef ref = entry.key_ref();
            if (ref.idx() > 0)
                return nullptr;

            KeySpan entry_key = m_data[ref];
            return key == entry_key ? m_data[ref.idx()] : nullptr;
        }

        if (entry.leaf()) {
            Leaf* leaf = entry.leaf_ptr();
            KeySpan leaf_key = m_data[leaf->m_refs[0]];
            if (key != leaf_key)
                return nullptr;

            /**
             * Check if key match first. If it does, find original string (where ref points to the
             * first char, because that is the whole string we are searching).
             */
            for (u32 i = 0; i < leaf->m_capacity; ++i)
                if (leaf->m_refs[i].idx() == 0)
                    return m_data[leaf->m_refs[i].idx()];

            return nullptr;
        }

        Node* node = entry.node_ptr();
        sz cp_len = node->common_header_prefix(key, depth);

        if (cp_len != hdrlen(node->m_prefix_len))
            return nullptr;

        depth += node->m_prefix_len;

        if (depth >= key.size())
            return nullptr;

        entry_ptr* next = node->find_child(key[depth]);
        if (next)
            return search(*next, key, depth + 1);

        return nullptr;
    }

    bool empty() const noexcept { return m_root; }

    void search_prefix(const KeySpan& key, std::vector<KeyValue*>& result, sz limit) const noexcept
    {
        if (m_root)
            search_prefix(m_root, key, 0, result, limit);
    }

    void search_prefix(const entry_ptr& entry, const KeySpan& prefix, sz depth,
                       std::vector<KeyValue*>& result, sz limit) const noexcept
    {
        if (entry.ref()) {
            KeyRef ref = entry.key_ref();
            KeySpan key = m_data[ref];

            if (prefix.match_prefix(key))
                result.push_back(m_data[ref.idx()]);

            return;
        }

        if (entry.leaf()) {
            Leaf* leaf = entry.leaf_ptr();
            KeySpan leaf_key = m_data[leaf->m_refs[0]];
            if (leaf_key.match_prefix(prefix))
                for (u32 i = 0; i < leaf->m_size; ++i)
                    result.push_back(m_data[leaf[i].idx()]);

            return;
        }

        Node* node = entry.node_ptr();
        sz cp_len = node->common_header_prefix(prefix, depth);

        // All bytes except terminal byte matched, so just collect leaves.
        //
        if (depth + cp_len == prefix.size() - 1) {
            for_each(entry, [&](const entry_ptr& ent) {
                if (ent.ref()) {
                    result.push_back(m_data[ent.key_ref().idx()]);
                    return result.size() < limit; // Limits number of iterations.
                }

                if (ent.leaf()) {
                    Leaf* leaf = ent.leaf_ptr();
                    for (u32 i = 0; i < leaf->m_size; ++i) {
                        result.push_back(m_data[ent.key_ref().idx()]);
                        if (result.size() >= limit) // Limits number of iterations.
                            return false;
                    }
                }

                return true;
            });

            return;
        }

        if (cp_len != hdrlen(node->m_prefix_len))
            return;

        depth += node->m_prefix_len;

        if (depth >= prefix.size())
            return;

        entry_ptr* next = node->find_child(prefix[depth]);
        if (next)
            search_prefix(*next, prefix, depth + 1, result, limit);
    }

    entry_ptr search_prefix_node(const KeySpan& key) const noexcept
    {
        if (m_root)
            return search_prefix_node(m_root, key, 0);

        return nullptr;
    }

    entry_ptr search_prefix_node(const entry_ptr& entry, const KeySpan& prefix,
                                 sz depth) const noexcept
    {
        if (entry.leaf())
            return entry.leaf_ptr()->match_prefix(prefix) ? entry : nullptr;

        Node* node = entry.node_ptr();
        sz cp_len = node->common_header_prefix(prefix, depth);

        // All bytes except terminal byte matched, so just return an entry.
        //
        if (depth + cp_len == prefix.size() - 1)
            return entry;

        if (cp_len != hdrlen(node->m_prefix_len))
            return nullptr;

        depth += node->m_prefix_len;

        if (depth >= prefix.size())
            return nullptr;

        entry_ptr* next = node->find_child(prefix[depth]);
        return next ? search_prefix_node(*next, prefix, depth + 1) : nullptr;
    }

    template<class Pred>
    void search_prefix_if(const KeySpan& key, std::vector<Leaf*>& leaves, Pred pred,
                          sz limit) const noexcept
    {
        if (m_root)
            search_prefix_if(m_root, key, 0, leaves, pred, limit);
    }

    template<class Pred>
    void search_prefix_if(const entry_ptr& entry, const KeySpan& prefix, sz depth,
                          std::vector<Leaf*>& leaves, Pred pred, sz limit) const noexcept
    {
        if (entry.leaf()) {
            Leaf* leaf = entry.leaf_ptr();
            if (leaf->match_prefix(prefix) && pred(leaf->value()))
                leaves.push_back(leaf);

            return;
        }

        Node* node = entry.node_ptr();
        sz cp_len = node->common_header_prefix(prefix, depth);

        // All bytes except terminal byte matched, so just collect leaves.
        //
        if (depth + cp_len == prefix.size() - 1) {
            for_each_leaf(entry, [&](Leaf* leaf) {
                if (pred(leaf->value()))
                    leaves.push_back(leaf);

                return leaves.size() < limit; // Limits number of iterations.
            });

            return;
        }

        if (cp_len != hdrlen(node->m_prefix_len))
            return;

        depth += node->m_prefix_len;

        if (depth >= prefix.size())
            return;

        entry_ptr* next = node->find_child(prefix[depth]);
        if (next)
            search_prefix_if(*next, prefix, depth + 1, leaves, pred, limit);
    }

public:
    entry_ptr m_root;
    Data m_data;
};

} // namespace ast

// NOLINTEND

#endif // AST_H