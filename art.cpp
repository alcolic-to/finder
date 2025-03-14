#include "art.h"

// NOLINTBEGIN

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

[[nodiscard]] entry_ptr Node4::find_child(uint8_t key) const noexcept
{
    for (int i = 0; i < 4; ++i)
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

Node* Node4::add_child(const Key& key) noexcept
{
    if (full())
        return grow()->add_child(key);

    size_t idx = 0;
    while (idx < m_num_children && m_keys[idx] < key[0])
        ++idx;

    // Make space for new child. Keep sorted order.
    //
    for (size_t i = m_num_children - 1; i > idx; --i) {
        m_keys[i] = m_keys[i - 1];
        m_children[i] = m_children[i - 1];
    }

    m_keys[idx] = key[0];
    m_children[idx] = new_leaf(key);

    ++m_num_children;
    return this;
}

[[nodiscard]] entry_ptr Node16::find_child(uint8_t key) const noexcept
{
    for (int i = 0; i < 16; ++i)
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

Node* Node16::add_child(const Key& key) noexcept
{
    if (full())
        return grow()->add_child(key);

    size_t idx = 0;
    while (idx < m_num_children && m_keys[idx] < key[0])
        ++idx;

    // Make space for new child. Keep sorted order.
    //
    for (size_t i = m_num_children - 1; i > idx; --i) {
        m_keys[i] = m_keys[i - 1];
        m_children[i] = m_children[i - 1];
    }

    m_keys[idx] = key[0];
    m_children[idx] = new_leaf(key);

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

Node* Node48::add_child(const Key& key) noexcept
{
    if (full())
        return grow()->add_child(key);

    assert(m_idxs[key[0]] == empty_slot);

    m_idxs[key[0]] = m_num_children;
    m_children[m_num_children] = new_leaf(key);

    ++m_num_children;
    return this;
}

[[nodiscard]] entry_ptr Node256::find_child(uint8_t key) const noexcept
{
    return m_children[key];
}

Node* Node256::add_child(const Key& key) noexcept
{
    assert(m_children[key[0]] == nullptr);

    m_children[key[0]] = new_leaf(key);
    ++m_num_children;

    return this;
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

Node* Node::add_child(const Key& key) noexcept
{
    switch (m_type) {
    case Node4_t:
        return static_cast<Node4*>(this)->add_child(key);
    case Node16_t:
        return static_cast<Node16*>(this)->add_child(key);
    case Node48_t:
        return static_cast<Node48*>(this)->add_child(key);
    case Node256_t:
        return static_cast<Node256*>(this)->add_child(key);
    default:
        return assert(!"Invalid case."), this;
    }
}

void ART::insert(uint8_t* data, size_t size) noexcept
{
    Key key{data, size};

    insert(m_root, key);
}

void ART::insert(entry_ptr& entry, Key& key) noexcept
{
    if (!entry) {
        entry = new_leaf(key);
        return;
    }

    if (entry.leaf()) {
        Leaf* leaf = entry.leaf_ptr();
        Node* new_node = new Node4{};

        new_node = new_node->add_child(key);
        new_node = new_node->add_child(Key{*leaf});

        delete leaf;
        entry = new_node;
        return;
    }

    assert(entry.node());
    Node* node = entry.node_ptr();
    entry_ptr next = node->find_child(key[0]);

    // TODO: Handle case where key is at the end and must create inner node.

    insert(next, ++key);
}

// NOLINTEND
