#include "art.h"

#include <cstring>

// NOLINTBEGIN

// Makes key from leaf node. We don't need to insert terminal byte at the end, because key already
// holds it.
//
Key::Key(Leaf& leaf) noexcept : m_data{leaf.m_key}, m_size{leaf.m_key_len} {}

// Not smart for now.
//
// entry_ptr::~entry_ptr() noexcept
// {
//     if (*this == nullptr) // invoke operator void* on this.
//         return;
//
//     if (node()) {
//         Node* n = node_ptr();
//
//         switch (n->m_type) {
//         case Node4_t:
//             delete (Node4*)n;
//             break;
//         case Node16_t:
//             delete (Node16*)n;
//             break;
//         case Node48_t:
//             delete (Node48*)n;
//             break;
//         case Node256_t:
//             delete (Node256*)n;
//             break;
//         default:
//             assert(!"Invalid case.");
//             delete n;
//             break;
//         }
//     }
//     else {
//         assert(leaf());
//         delete leaf_ptr();
//     }
// }

// Creates new node4 as a parent for key(future leaf node) and leaf. New node will have 2
// children with common prefix extracted from them.
//
Node4::Node4(const Key& key, size_t depth, Leaf* leaf) : Node{Node4_t}
{
    size_t offset = depth;
    size_t bytes_copied = 0;

    while (key[offset] == leaf->m_key[offset]) {
        m_prefix[bytes_copied] = key[offset];
        ++offset, ++bytes_copied;
    }

    m_prefix_len = bytes_copied;
    depth += bytes_copied;

    add_child(key[depth], Leaf::new_leaf(key));
    add_child(leaf->m_key[depth], leaf);
}

// Creates new node4 as a parent for key(future leaf node) and node. New node will have 2
// children with common prefix extracted from them. We must also delete taken prefix from child
// node, plus we must take one additional byte from prefix as a key for child node.
//
Node4::Node4(const Key& key, size_t depth, Node* node) : Node{Node4_t}
{
    size_t cp_len = node->common_prefix(key, depth);

    std::memcpy(m_prefix, node->m_prefix, cp_len);
    m_prefix_len = cp_len;

    uint8_t node_key_byte = node->m_prefix[cp_len];
    size_t copy_bytes = node->m_prefix_len - cp_len - 1;

    std::memmove(node->m_prefix, node->m_prefix + cp_len + 1, copy_bytes);
    node->m_prefix_len = copy_bytes;

    add_child(key[depth + cp_len], Leaf::new_leaf(key));
    add_child(node_key_byte, node);
}

[[nodiscard]] entry_ptr* Node::find_child(uint8_t key) noexcept
{
    switch (m_type) {
    case Node4_t:
        return static_cast<Node4*>(this)->find_child(key);
    case Node16_t:
        return static_cast<Node16*>(this)->find_child(key);
    case Node48_t:
        return static_cast<Node48*>(this)->find_child(key);
    case Node256_t:
        return static_cast<Node256*>(this)->find_child(key);
    default:
        return assert(!"Invalid case."), nullptr;
    }
}

[[nodiscard]] entry_ptr* Node4::find_child(uint8_t key) noexcept
{
    for (int i = 0; i < m_num_children; ++i)
        if (m_keys[i] == key)
            return &m_children[i];

    return nullptr;
}

[[nodiscard]] entry_ptr* Node16::find_child(uint8_t key) noexcept
{
    for (int i = 0; i < m_num_children; ++i)
        if (m_keys[i] == key)
            return &m_children[i];

    return nullptr;
}

[[nodiscard]] entry_ptr* Node48::find_child(uint8_t key) noexcept
{
    return m_idxs[key] != empty_slot ? &m_children[m_idxs[key]] : nullptr;
}

[[nodiscard]] entry_ptr* Node256::find_child(uint8_t key) noexcept
{
    return &m_children[key];
}

void Node::add_child(const uint8_t key, entry_ptr child) noexcept
{
    switch (m_type) {
    case Node4_t:
        return static_cast<Node4*>(this)->add_child(key, child);
    case Node16_t:
        return static_cast<Node16*>(this)->add_child(key, child);
    case Node48_t:
        return static_cast<Node48*>(this)->add_child(key, child);
    case Node256_t:
        return static_cast<Node256*>(this)->add_child(key, child);
    default:
        return assert(!"Invalid case.");
    }
}

void Node4::add_child(const uint8_t key, entry_ptr child) noexcept
{
    assert(m_num_children < 4);

    size_t idx = 0;
    while (idx < m_num_children && m_keys[idx] < key)
        ++idx;

    // Make space for new child. Keep sorted order.
    //
    for (size_t i = m_num_children; i > idx; --i) {
        m_keys[i] = m_keys[i - 1];
        m_children[i] = m_children[i - 1];
    }

    m_keys[idx] = key;
    m_children[idx] = child;

    ++m_num_children;
}

void Node16::add_child(const uint8_t key, entry_ptr child) noexcept
{
    assert(m_num_children < 16);

    size_t idx = 0;
    while (idx < m_num_children && m_keys[idx] < key)
        ++idx;

    // Make space for new child. Keep sorted order.
    //
    for (size_t i = m_num_children; i > idx; --i) {
        m_keys[i] = m_keys[i - 1];
        m_children[i] = m_children[i - 1];
    }

    m_keys[idx] = key;
    m_children[idx] = child;

    ++m_num_children;
}

void Node48::add_child(const uint8_t key, entry_ptr child) noexcept
{
    assert(m_num_children < 48);
    assert(m_idxs[key] == empty_slot);

    m_idxs[key] = m_num_children;
    m_children[m_num_children] = child;

    ++m_num_children;
}

void Node256::add_child(const uint8_t key, entry_ptr child) noexcept
{
    assert(m_num_children < 255);
    assert(m_children[key] == nullptr);

    m_children[key] = child;
    ++m_num_children;
}

[[nodiscard]] bool Node::full() const noexcept
{
    switch (m_type) {
    case Node4_t:
        return static_cast<const Node4*>(this)->full();
    case Node16_t:
        return static_cast<const Node16*>(this)->full();
    case Node48_t:
        return static_cast<const Node48*>(this)->full();
    case Node256_t:
        return static_cast<const Node256*>(this)->full();
    default:
        return assert(!"Invalid case."), false;
    }
}

[[nodiscard]] Node* Node::grow() noexcept
{
    switch (m_type) {
    case Node4_t:
        return static_cast<Node4*>(this)->grow();
    case Node16_t:
        return static_cast<Node16*>(this)->grow();
    case Node48_t:
        return static_cast<Node48*>(this)->grow();
    default:
        return assert(!"Invalid case."), this;
    }
}

[[nodiscard]] Node16* Node4::grow() noexcept
{
    Node16* new_node = new Node16{*this};
    delete this;

    return new_node;
}

[[nodiscard]] Node48* Node16::grow() noexcept
{
    Node48* new_node = new Node48{*this};

    delete this;
    return new_node;
}

[[nodiscard]] Node256* Node48::grow() noexcept
{
    Node256* new_node = new Node256{*this};

    for (int i = 0; i < 256; ++i)
        if (m_idxs[i] != empty_slot)
            new_node->m_children[i] = m_children[m_idxs[i]];

    delete this;
    return new_node;
}

void ART::insert(uint8_t* data, size_t size) noexcept
{
    Key key{data, size};

    if (!m_root)
        m_root = Leaf::new_leaf(key);
    else
        insert(m_root, key, 0);
}

void ART::insert(entry_ptr& entry, const Key& key, size_t depth) noexcept
{
    assert(entry);

    if (entry.leaf()) {
        entry = new Node4{key, depth, entry.leaf_ptr()};
        return;
    }

    assert(entry.node());
    Node* node = entry.node_ptr();

    size_t cp_len = node->common_prefix(key, depth);
    if (cp_len < node->m_prefix_len) {
        entry = new Node4{key, depth, node};
        return;
    }

    assert(cp_len == node->m_prefix_len);
    depth += node->m_prefix_len;

    entry_ptr* next = node->find_child(key[depth]);
    if (next)
        return insert(*next, key, depth + 1);

    if (node->full())
        node = node->grow();

    node->add_child(key[depth], Leaf::new_leaf(key));
}

// NOLINTEND
