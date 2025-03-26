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
    size_t max_depth = std::min(key.size() - 1, leaf->m_key_len - 1);

    while (depth < max_depth && key[depth] == leaf->m_key[depth]) {
        if (m_prefix_len < max_prefix_len)
            m_prefix[m_prefix_len] = key[depth];

        ++m_prefix_len, ++depth;
    }

    assert(depth < key.size() && depth < leaf->m_key_len);
    assert(key[depth] != leaf->m_key[depth]);

    add_child(key[depth], Leaf::new_leaf(key));
    add_child(leaf->m_key[depth], leaf);
}

// Creates new node4 as a parent for key(future leaf node) and node. New node will have 2
// children with common prefix extracted from them. We must also delete taken prefix from child
// node, plus we must take one additional byte from prefix as a key for child node.
//
Node4::Node4(const Key& key, size_t depth, Node* node, size_t cp_len) : Node{Node4_t}
{
    assert(cp_len < node->m_prefix_len);

    m_prefix_len = cp_len;
    std::memcpy(m_prefix, node->m_prefix, std::min(cp_len, (size_t)max_prefix_len));

    assert(node->m_prefix_len >= cp_len + 1);
    node->m_prefix_len -= (cp_len + 1); // Additional byte for mapping.
    size_t header_bytes = std::min(node->m_prefix_len, max_prefix_len);

    // Adjust old node header prefix. Check if we can copy new prefix from node header, to avoid
    // going to leaf.
    //
    if (node->m_prefix_len + cp_len + 1 <= max_prefix_len) {
        uint8_t node_key = node->m_prefix[cp_len];
        std::memmove(node->m_prefix, node->m_prefix + cp_len + 1, header_bytes);

        assert(node_key != key[depth + cp_len]);
        add_child(node_key, node);
    }
    else {
        const Leaf& leaf = node->next_leaf();
        std::memcpy(node->m_prefix, &leaf[depth + cp_len + 1], header_bytes);

        assert(leaf[depth + cp_len] != key[depth + cp_len]);
        add_child(leaf[depth + cp_len], node);
    }

    add_child(key[depth + cp_len], Leaf::new_leaf(key));
}

// Collapses node4 by replacing it with child entry (which is the only child in this node). This
// process is reverse of Node4 creation from 2 children entries.
// If child entry is a node, all bytes that can fit in child's header are copied (key for mapping
// first and header prefix after). Since we must use prefix bytes from our header as first bytes in
// new header, so we will fill our header buffer with missing bytes from child header and copy it to
// child header, because our node will get destroyed anyway.
// If child is a leaf, there is nothing to do - we will just return it.
//
[[nodiscard]] entry_ptr Node4::collapse() noexcept
{
    assert(m_num_children == 1);
    assert(m_children[0]);

    uint8_t child_key = m_keys[0];
    entry_ptr child_entry{m_children[0]};

    if (child_entry.node()) {
        Node* child_node = child_entry.node_ptr();
        uint32_t hdr_pref_len = m_prefix_len;

        if (hdr_pref_len < max_prefix_len)
            m_prefix[hdr_pref_len++] = child_key;

        if (hdr_pref_len < max_prefix_len)
            std::memcpy(m_prefix + hdr_pref_len, child_node->m_prefix,
                        std::min(max_prefix_len - hdr_pref_len, child_node->m_prefix_len));

        std::memcpy(child_node->m_prefix, m_prefix, std::min(hdr_pref_len, max_prefix_len));
        child_node->m_prefix_len += m_prefix_len + 1;
    }

    delete this;
    return child_entry;
}

Node4::Node4(Node16& old_node) noexcept : Node{old_node, Node4_t}
{
    std::memcpy(m_keys, old_node.m_keys, old_node.m_num_children);
    std::memcpy(m_children, old_node.m_children, old_node.m_num_children * sizeof(entry_ptr));
}

Node16::Node16(Node4& old_node) noexcept : Node{old_node, Node16_t}
{
    std::memcpy(m_keys, old_node.m_keys, 4);
    std::memcpy(m_children, old_node.m_children, 4 * sizeof(entry_ptr));
}

Node16::Node16(Node48& old_node) noexcept : Node{old_node, Node16_t}
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

Node48::Node48(Node16& old_node) noexcept : Node{old_node, Node48_t}
{
    std::memset(m_idxs, empty_slot, 256);
    std::memcpy(m_children, old_node.m_children, 16 * sizeof(entry_ptr));

    for (size_t i = 0; i < 16; ++i)
        m_idxs[old_node.m_keys[i]] = i;
}

Node48::Node48(Node256& old_node) noexcept : Node{old_node, Node48_t}
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

Node256::Node256(const Node48& old_node) noexcept : Node{old_node, Node256_t}
{
    for (size_t i = 0; i < 256; ++i)
        if (old_node.m_idxs[i] != Node48::empty_slot)
            m_children[i] = old_node.m_children[old_node.m_idxs[i]];
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
    return m_children[key] ? &m_children[key] : nullptr;
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

    size_t idx = 0;
    while (m_children[idx])
        ++idx;

    m_idxs[key] = idx;
    m_children[idx] = child;

    ++m_num_children;
}

void Node256::add_child(const uint8_t key, entry_ptr child) noexcept
{
    assert(m_children[key] == nullptr);

    m_children[key] = child;
    ++m_num_children;
}

void Node::remove_child(const uint8_t key) noexcept
{
    switch (m_type) {
    case Node4_t:
        return static_cast<Node4*>(this)->remove_child(key);
    case Node16_t:
        return static_cast<Node16*>(this)->remove_child(key);
    case Node48_t:
        return static_cast<Node48*>(this)->remove_child(key);
    case Node256_t:
        return static_cast<Node256*>(this)->remove_child(key);
    default:
        return assert(!"Invalid case.");
    }
}

void Node4::remove_child(const uint8_t key) noexcept
{
    size_t idx = 0;
    while (m_keys[idx] != key)
        ++idx;

    assert(idx < m_num_children);
    delete m_children[idx].leaf_ptr();

    for (; idx < m_num_children - 1; ++idx) {
        m_keys[idx] = m_keys[idx + 1];
        m_children[idx] = m_children[idx + 1];
    }

    m_keys[idx] = 0;
    m_children[idx] = entry_ptr{};

    --m_num_children;
}

void Node16::remove_child(const uint8_t key) noexcept
{
    size_t idx = 0;
    while (m_keys[idx] != key)
        ++idx;

    assert(idx < m_num_children);
    delete m_children[idx].leaf_ptr();

    for (; idx < m_num_children - 1; ++idx) {
        m_keys[idx] = m_keys[idx + 1];
        m_children[idx] = m_children[idx + 1];
    }

    m_keys[idx] = 0;
    m_children[idx] = entry_ptr{};

    --m_num_children;
}

void Node48::remove_child(const uint8_t key) noexcept
{
    assert(m_idxs[key] != empty_slot);

    size_t idx = m_idxs[key];
    m_idxs[key] = empty_slot;

    delete m_children[idx].leaf_ptr();
    m_children[idx] = entry_ptr{};

    --m_num_children;
}

void Node256::remove_child(const uint8_t key) noexcept
{
    assert(m_children[key]);

    delete m_children[key].leaf_ptr();
    m_children[key] = entry_ptr{};

    --m_num_children;
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

[[nodiscard]] bool Node::should_shrink() const noexcept
{
    switch (m_type) {
    case Node4_t:
        return static_cast<const Node4*>(this)->should_shrink();
    case Node16_t:
        return static_cast<const Node16*>(this)->should_shrink();
    case Node48_t:
        return static_cast<const Node48*>(this)->should_shrink();
    case Node256_t:
        return static_cast<const Node256*>(this)->should_shrink();
    default:
        return assert(!"Invalid case."), false;
    }
}

[[nodiscard]] bool Node::should_collapse() const noexcept
{
    return m_type == Node4_t && m_num_children == 1;
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

    delete this;
    return new_node;
}

[[nodiscard]] Node* Node::shrink() noexcept
{
    switch (m_type) {
    case Node16_t:
        return static_cast<Node16*>(this)->shrink();
    case Node48_t:
        return static_cast<Node48*>(this)->shrink();
    case Node256_t:
        return static_cast<Node256*>(this)->shrink();
    default:
        return assert(!"Invalid case."), this;
    }
}

[[nodiscard]] Node4* Node16::shrink() noexcept
{
    Node4* new_node = new Node4{*this};

    delete this;
    return new_node;
}

[[nodiscard]] Node16* Node48::shrink() noexcept
{
    Node16* new_node = new Node16{*this};

    delete this;
    return new_node;
}

[[nodiscard]] Node48* Node256::shrink() noexcept
{
    Node48* new_node = new Node48{*this};

    delete this;
    return new_node;
}

[[nodiscard]] entry_ptr Node::collapse() noexcept
{
    switch (m_type) {
    case Node4_t:
        return static_cast<Node4*>(this)->collapse();
    default:
        return assert(!"Invalid case."), this;
    }
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

// Returns next leaf from current entry. If current entry is leaf, it returns it. Otherwise, it
// calls next_leaf() on a node which will recurse down one level based on node type an call us
// again.
//
[[nodiscard]] inline const Leaf& entry_ptr::next_leaf() const noexcept
{
    assert(*this != nullptr);
    return leaf() ? *leaf_ptr() : node_ptr()->next_leaf();
}

[[nodiscard]] const Leaf& Node4::next_leaf() const noexcept
{
    return m_children[0].next_leaf();
}

[[nodiscard]] const Leaf& Node16::next_leaf() const noexcept
{
    return m_children[0].next_leaf();
}

[[nodiscard]] const Leaf& Node48::next_leaf() const noexcept
{
    for (int i = 0; i < 256; ++i)
        if (m_idxs[i] != empty_slot)
            return m_children[m_idxs[i]].next_leaf();

    assert(false);
    return *Leaf::new_leaf(Key{nullptr, 0});
}

[[nodiscard]] const Leaf& Node256::next_leaf() const noexcept
{
    for (int i = 0; i < 256; ++i)
        if (m_children[i])
            return m_children[i].next_leaf();

    assert(false);
    return *Leaf::new_leaf(Key{nullptr, 0});
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

// Returns common prefix length from this node to the leaf and a key at provided depth.
// If prefix in our header is max_prefix_len long and we matched whole prefix, we need to find leaf
// to keep comparing. It is not important which leaf we take, because all of them have at least
// m_prefix_len common bytes.
//
[[nodiscard]] size_t Node::common_prefix(const Key& key, size_t depth) const noexcept
{
    size_t cp_len = common_header_prefix(key, depth);

    if (cp_len < m_prefix_len && cp_len == max_prefix_len && cp_len < key.size() - depth) {
        const Leaf& leaf = next_leaf();
        const size_t max_cmp = std::min(key.size() - depth, (size_t)m_prefix_len);
        while (cp_len < max_cmp && key[depth + cp_len] == leaf[depth + cp_len])
            ++cp_len;
    }

    return cp_len;
}

// Handles insertion when we reached leaf node. With current implementation, we will only insert new
// key if it differs from an exitisting leaf.
// TODO: Maybe implement substitution of the old entry.
//
void ART::insert_at_leaf(entry_ptr& entry, const Key& key, size_t depth) noexcept
{
    Leaf* leaf = entry.leaf_ptr();
    if (!leaf->match(key))
        entry = new Node4{key, depth, leaf};
}

// Handles insertion when we reached innder node and key at provided depth did not match full node
// prefix.
//
void ART::insert_at_node(entry_ptr& entry, const Key& key, size_t depth, size_t cp_len) noexcept
{
    Node* node = entry.node_ptr();
    assert(cp_len != node->m_prefix_len);

    entry = new Node4{key, depth, node, cp_len};
}

// Insert new key into the tree.
//
void ART::insert(const Key& key) noexcept
{
    if (m_root)
        return insert(m_root, key, 0);

    m_root = Leaf::new_leaf(key);
}

// Inserts new key into the tree.
//
void ART::insert(entry_ptr& entry, const Key& key, size_t depth) noexcept
{
    assert(entry);

    if (entry.leaf())
        return insert_at_leaf(entry, key, depth);

    Node* node = entry.node_ptr();
    size_t cp_len = node->common_prefix(key, depth);

    if (cp_len != node->m_prefix_len)
        return insert_at_node(entry, key, depth, cp_len);

    assert(cp_len == node->m_prefix_len);
    depth += node->m_prefix_len;

    entry_ptr* next = node->find_child(key[depth]);
    if (next)
        return insert(*next, key, depth + 1);

    if (node->full())
        entry = node = node->grow();

    node->add_child(key[depth], Leaf::new_leaf(key));
}

void ART::erase(const Key& key) noexcept
{
    if (!m_root)
        return;

    if (m_root.node())
        return erase(m_root, key, 0);

    Leaf* leaf = m_root.leaf_ptr();
    if (!leaf->match(key))
        return;

    delete leaf;
    m_root = entry_ptr{};
}

// Erases leaf node.
// If remaining children are below some treshold we will shrink node to save space.
// If we have only 1 child left in node, we will replace node with it's child (collapse it).
//
void ART::erase_leaf(entry_ptr& entry, Leaf* leaf, const Key& key, size_t depth) noexcept
{
    if (!leaf->match(key))
        return;

    Node* node = entry.node_ptr();
    node->remove_child(key[depth]);

    if (node->should_shrink())
        entry = node = node->shrink();

    if (node->should_collapse())
        entry = node->collapse();
}

// Delete: The implementation of deletion is symmetrical to
// insertion.The leaf is removed from an inner node,
// which is shrunk if necessary. If that node now has only one child,
// it is replaced by its child and the compressed path is adjusted.
//
void ART::erase(entry_ptr& entry, const Key& key, size_t depth) noexcept
{
    Node* node = entry.node_ptr();
    size_t cp_len = node->common_header_prefix(key, depth);

    if (cp_len != std::min(node->m_prefix_len, Node::max_prefix_len))
        return;

    depth += node->m_prefix_len;

    entry_ptr* next = node->find_child(key[depth]);
    if (!next)
        return;

    if (next->node())
        return erase(*next, key, depth + 1);

    erase_leaf(entry, next->leaf_ptr(), key, depth);
}

Leaf* ART::search(const Key& key) noexcept
{
    if (m_root)
        return search(m_root, key, 0);

    return nullptr;
}

Leaf* ART::search(entry_ptr& entry, const Key& key, size_t depth) noexcept
{
    if (entry.leaf()) {
        Leaf* leaf = entry.leaf_ptr();
        if (leaf->match(key))
            return leaf;

        return nullptr;
    }

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
