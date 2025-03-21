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
// children with common prefix extracted from them. We will store information about common prefix
// len in prefix_len, but we can only copy up to max_prefix_bytes in prefix array. Later when
// searching for a key, we will compare all bytes from our prefix array, and if all bytes matches,
// we will just skip all prefix bytes and go to directly to next node. This is hybrid approach which
// mixes optimistic and pesimistic approaches.
//
Node4::Node4(const Key& key, size_t depth, Leaf* leaf) : Node{Node4_t}
{
    m_prefix_len = 0;
    size_t max_depth = std::min(key.size(), leaf->m_key_len);

    while (depth < max_depth && key[depth] == leaf->m_key[depth]) {
        if (m_prefix_len < max_prefix_len)
            m_prefix[m_prefix_len] = key[depth];

        ++m_prefix_len, ++depth;
    }

    add_child(key[depth], Leaf::new_leaf(key));
    add_child(leaf->m_key[depth], leaf);
}

// Creates new node4 as a parent for key(future leaf node) and node. New node will have 2
// children with common prefix extracted from them. We must also delete taken prefix from child
// node, plus we must take one additional byte from prefix as a key for child node.
//
Node4::Node4(const Key& key, size_t depth, Node* node, size_t cp_len) : Node{Node4_t}
{
    assert(cp_len > 0 && cp_len < node->m_prefix_len);

    m_prefix_len = cp_len;
    std::memcpy(m_prefix, node->m_prefix, std::min(cp_len, (size_t)max_prefix_len));

    node->m_prefix_len -= (cp_len + 1); // Additional byte for mapping.
    size_t header_bytes = std::min(node->m_prefix_len, max_prefix_len);

    // Adjust old node header prefix. Check if we can copy new prefix from node header, to avoid
    // going to leaf.
    //
    if (node->m_prefix_len + cp_len + 1 <= max_prefix_len) {
        uint8_t node_key = node->m_prefix[cp_len];
        std::memmove(node->m_prefix, node->m_prefix + cp_len + 1, header_bytes);

        add_child(node_key, node);
    }
    else {
        const Leaf& leaf = node->next_leaf();
        std::memcpy(node->m_prefix, &leaf[depth + cp_len + 1], header_bytes);

        add_child(leaf[depth + cp_len], node);
    }

    add_child(key[depth + cp_len], Leaf::new_leaf(key));
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

[[nodiscard]] const Leaf& Node::next_leaf() const noexcept
{
    switch (m_type) {
    case Node4_t:
        return static_cast<const Node4*>(this)->next_leaf();
    case Node16_t:
        return static_cast<const Node16*>(this)->next_leaf();
    case Node48_t:
        return static_cast<const Node48*>(this)->next_leaf();
    case Node256_t:
        return static_cast<const Node256*>(this)->next_leaf();
    default:
        return assert(!"Invalid case."), *Leaf::new_leaf(Key{nullptr, 0});
    }
}

[[nodiscard]] const Leaf& Node4::next_leaf() const noexcept
{
    entry_ptr entry = m_children[0];
    return entry.leaf() ? *entry.leaf_ptr() : entry.node_ptr()->next_leaf();
}

[[nodiscard]] const Leaf& Node16::next_leaf() const noexcept
{
    entry_ptr entry = m_children[0];
    return entry.leaf() ? *entry.leaf_ptr() : entry.node_ptr()->next_leaf();
}

[[nodiscard]] const Leaf& Node48::next_leaf() const noexcept
{
    for (int i = 0; i < 256; ++i) {
        if (m_idxs[i] != empty_slot) {
            entry_ptr entry = m_children[m_idxs[i]];
            return entry.leaf() ? *entry.leaf_ptr() : entry.node_ptr()->next_leaf();
        }
    }

    assert(false);
    return *Leaf::new_leaf(Key{nullptr, 0});
}

[[nodiscard]] const Leaf& Node256::next_leaf() const noexcept
{
    for (int i = 0; i < 256; ++i) {
        entry_ptr entry = m_children[i];
        if (entry)
            return entry.leaf() ? *entry.leaf_ptr() : entry.node_ptr()->next_leaf();
    }

    assert(false);
    return *Leaf::new_leaf(Key{nullptr, 0});
}

// Returns common prefix length from this node to the leaf and a key at provided depth.
// If prefix in our header is max_prefix_len long and we matched whole prefix, we need to find leaf
// to keep comparing. It is not important which leaf we take, because all of them have at least
// m_prefix_len common bytes.
//
[[nodiscard]] size_t Node::common_prefix(const Key& key, size_t depth) const noexcept
{
    size_t cp_len = common_header_prefix(key, depth);

    if (cp_len == max_prefix_len) {
        const Leaf& leaf = next_leaf();
        const size_t max_cmp = std::min(key.size() - depth, (size_t)m_prefix_len);
        while (cp_len < max_cmp && key[depth + cp_len] == leaf[depth + cp_len])
            ++cp_len;
    }

    return cp_len;
}

// Returns common prefix len for key at current depth and a prefix in our header.
//
[[nodiscard]] size_t Node::common_header_prefix(const Key& key, size_t depth) const noexcept
{
    const size_t prefix_cmp = std::min(m_prefix_len, max_prefix_len);
    const size_t max_cmp = std::min(key.size() - depth, prefix_cmp);
    size_t cp_len = 0;

    while (cp_len < max_cmp && key[depth + cp_len] == m_prefix[cp_len])
        ++cp_len;

    return cp_len;
}

void ART::insert(const std::string& s) noexcept
{
    const Key key{(const uint8_t* const)s.data(), s.size()};
    insert(key);
}

void ART::insert(const uint8_t* const data, size_t size) noexcept
{
    const Key key{data, size};
    insert(key);
}

// Insert new key into the tree.
//
void ART::insert(const Key& key) noexcept
{
    if (!m_root)
        m_root = Leaf::new_leaf(key);
    else
        insert(m_root, key, 0);
}

// Inserts new key into the tree.
//
void ART::insert(entry_ptr& entry, const Key& key, size_t depth) noexcept
{
    assert(entry);

    if (entry.leaf()) {
        entry = new Node4{key, depth, entry.leaf_ptr()};
        return;
    }

    assert(entry.node());
    Node* node = entry.node_ptr();

    size_t cp_len = node->common_header_prefix(key, depth);
    if (cp_len != node->m_prefix_len) {
        if (cp_len == Node::max_prefix_len)
            cp_len = node->common_prefix(key, depth);

        if (cp_len != node->m_prefix_len) {
            entry = new Node4{key, depth, node, cp_len};
            return;
        }
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

Leaf* ART::search(const std::string& s) noexcept
{
    const Key key{(const uint8_t* const)s.data(), s.size()};
    return search(key);
}

Leaf* ART::search(const uint8_t* const data, size_t size) noexcept
{
    const Key key{data, size};
    return search(key);
}

Leaf* ART::search(const Key& key) noexcept
{
    if (!m_root)
        return nullptr;

    return search(m_root, key, 0);
}

Leaf* ART::search(entry_ptr& entry, const Key& key, size_t depth) noexcept
{
    if (entry.leaf()) {
        Leaf* leaf = entry.leaf_ptr();
        if (leaf->match(key))
            return leaf;

        return nullptr;
    }

    assert(entry.node());
    Node* node = entry.node_ptr();

    size_t cp_len = node->common_header_prefix(key, depth);
    if (cp_len != std::min(node->m_prefix_len, Node::max_prefix_len))
        return nullptr;

    depth += node->m_prefix_len;

    entry_ptr* next = node->find_child(key[depth]);
    if (next)
        return search(*next, key, depth + 1);

    return nullptr;
}

// NOLINTEND
