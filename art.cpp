#include "art.h"

#include <cstring>

// NOLINTBEGIN

// Makes key from leaf node. We don't need to insert terminal byte at the end, because key already
// holds it.
//
Key::Key(Leaf& leaf) noexcept : m_data{leaf.m_key}, m_size{leaf.m_key_len} {}

entry_ptr::~entry_ptr() noexcept
{
    if (*this == nullptr) // invoke operator void* on this.
        return;

    if (node()) {
        Node* n = node_ptr();

        switch (n->m_type) {
        case Node4_t:
            delete (Node4*)n;
            break;
        case Node16_t:
            delete (Node16*)n;
            break;
        case Node48_t:
            delete (Node48*)n;
            break;
        case Node256_t:
            delete (Node256*)n;
            break;
        default:
            assert(!"Invalid case.");
            delete n;
            break;
        }
    }
    else {
        assert(leaf());
        delete leaf_ptr();
    }
}

[[nodiscard]] entry_ptr Node::find_child(uint8_t key) noexcept
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
        return assert(!"Invalid case."), this;
    }
}

Node* Node::add_child(const uint8_t key, const entry_ptr& child) noexcept
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
        return assert(!"Invalid case."), this;
    }
}

[[nodiscard]] entry_ptr Node4::find_child(uint8_t key) const noexcept
{
    for (int i = 0; i < m_num_children; ++i)
        if (m_keys[i] == key)
            return m_children[i];

    return entry_ptr{};
}

[[nodiscard]] Node16* Node4::grow() noexcept
{
    Node16* new_node = new Node16{*this};
    delete this;

    return new_node;
}

[[nodiscard]] bool Node4::full() const noexcept
{
    return m_num_children == 4;
}

Node* Node4::add_child(const uint8_t key, const entry_ptr& child) noexcept
{
    if (full())
        return grow()->add_child(key, child);

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
    return this;
}

[[nodiscard]] entry_ptr Node16::find_child(uint8_t key) const noexcept
{
    for (int i = 0; i < m_num_children; ++i)
        if (m_keys[i] == key)
            return m_children[i];

    return entry_ptr{};
}

[[nodiscard]] Node48* Node16::grow() noexcept
{
    Node48* new_node = new Node48{};

    std::memcpy(new_node->m_children, m_children, 16);
    for (int i = 0; i < 16; ++i)
        new_node->m_idxs[m_keys[i]] = i;

    delete this;
    return new_node;
}

[[nodiscard]] bool Node16::full() const noexcept
{
    return m_num_children == 16;
}

Node* Node16::add_child(const uint8_t key, const entry_ptr& child) noexcept
{
    if (full())
        return grow()->add_child(key, child);

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
    return this;
}

[[nodiscard]] entry_ptr Node48::find_child(uint8_t key) const noexcept
{
    return m_idxs[key] != empty_slot ? m_children[m_idxs[key]] : entry_ptr{};
}

[[nodiscard]] Node256* Node48::grow() noexcept
{
    Node256* new_node = new Node256{};

    for (int i = 0; i < 256; ++i)
        if (m_idxs[i] != empty_slot)
            new_node->m_children[i] = m_children[m_idxs[i]];

    delete this;
    return new_node;
}

[[nodiscard]] bool Node48::full() const noexcept
{
    return m_num_children == 48;
}

Node* Node48::add_child(const uint8_t key, const entry_ptr& child) noexcept
{
    if (full())
        return grow()->add_child(key, child);

    assert(m_idxs[key] == empty_slot);

    m_idxs[key] = m_num_children;
    m_children[m_num_children] = child;

    ++m_num_children;
    return this;
}

[[nodiscard]] entry_ptr Node256::find_child(uint8_t key) const noexcept
{
    return m_children[key];
}

Node* Node256::add_child(const uint8_t key, const entry_ptr& child) noexcept
{
    assert(m_children[key] == nullptr);

    m_children[key] = child;
    ++m_num_children;

    return this;
}

void ART::insert(uint8_t* data, size_t size) noexcept
{
    Key key{data, size};

    insert(m_root, key, 0);
}

void ART::insert(entry_ptr& entry, Key& key, size_t depth) noexcept
{
    if (!entry) {
        entry = Leaf::new_leaf(key);
        return;
    }

    if (entry.leaf()) {
        Leaf* leaf = entry.leaf_ptr();
        Node* new_node = new Node4{};

        // Take common prefix from current depth to the end.
        //
        size_t offset = 0, bytes_copied = 0;
        for (offset = depth; key[offset] == leaf->m_key[offset]; ++offset, ++bytes_copied)
            new_node->m_prefix[bytes_copied] = key[offset];

        new_node->m_prefix_len = bytes_copied;
        depth += bytes_copied;

        new_node = new_node->add_child(key[depth], Leaf::new_leaf(key));
        new_node = new_node->add_child(leaf->m_key[depth], leaf);

        entry = new_node;

        return;
    }

    assert(entry.node());
    Node* node = entry.node_ptr();

    size_t common_prefix_len = node->common_prefix(key, depth);
    if (common_prefix_len < node->m_prefix_len) {
        Node* new_node = new Node4{};

        std::memcpy(new_node->m_prefix, node->m_prefix, common_prefix_len);
        new_node->m_prefix_len = common_prefix_len;

        // We must take additional byte from the old node prefix for key mapping in a new node.
        //
        uint8_t node_key_byte = node->m_prefix[common_prefix_len];
        size_t copy_bytes = node->m_prefix_len - common_prefix_len - 1;

        std::memmove(node->m_prefix, node->m_prefix + common_prefix_len + 1, copy_bytes);
        node->m_prefix_len = copy_bytes;

        new_node = new_node->add_child(key[depth + common_prefix_len], Leaf::new_leaf(key));
        new_node = new_node->add_child(node_key_byte, node);

        entry = new_node;

        return;
    }

    assert(common_prefix_len == node->m_prefix_len);
    depth += node->m_prefix_len;

    entry_ptr next = node->find_child(key[depth]);
    if (next)
        insert(next, key, depth + 1);
    else
        node = node->add_child(key[depth], Leaf::new_leaf(key));
}

// NOLINTEND
