#include "art.h"

#include <cstring>

// NOLINTBEGIN

// Construct new node from previous node by copying whole header except node type which is
// provided as new_type.
//
Node::Node(const Node& other, uint8_t new_type)
    : m_prefix_len{other.m_prefix_len}
    , m_num_children{other.m_num_children}
    , m_type{new_type}
{
    if (other.m_prefix_len > 0)
        std::memcpy(m_prefix, other.m_prefix, hdrlen(other.m_prefix_len));
}

// Creates new node4 as a parent for key(future leaf node) and leaf. New node will have 2
// children with common prefix extracted from them. We will store information about common prefix
// len in prefix_len, but we can only copy up to max_prefix_bytes in prefix array. Later when
// searching for a key, we will compare all bytes from our prefix array, and if all bytes matches,
// we will just skip all prefix bytes and go to directly to next node. This is hybrid approach which
// mixes optimistic and pessimistic approaches.
//
Node4::Node4(const Key& key, void* value, size_t depth, Leaf* leaf) : Node{Node4_t}
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
// If node prefix does not fit into header, we must go to leaf node (not important which one), to
// take prefix bytes from there. It it fits, we can take bytes from header directly.
//
Node4::Node4(const Key& key, void* value, size_t depth, Node* node, size_t cp_len) : Node{Node4_t}
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

    entry_ptr child_entry{m_children[0]};
    if (child_entry.node()) {
        Node* child_node = child_entry.node_ptr();

        uint32_t len = hdrlen(m_prefix_len);
        if (len < max_prefix_len)
            m_prefix[len++] = m_keys[0]; // Key for mapping.

        for (uint32_t i = 0; len < max_prefix_len && i < child_node->m_prefix_len;)
            m_prefix[len++] = child_node->m_prefix[i++];

        std::memcpy(child_node->m_prefix, m_prefix, len);
        child_node->m_prefix_len += m_prefix_len + 1;
    }

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
        return node4()->find_child(key);
    case Node16_t:
        return node16()->find_child(key);
    case Node48_t:
        return node48()->find_child(key);
    case Node256_t:
        return node256()->find_child(key);
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
        return node4()->add_child(key, child);
    case Node16_t:
        return node16()->add_child(key, child);
    case Node48_t:
        return node48()->add_child(key, child);
    case Node256_t:
        return node256()->add_child(key, child);
    default:
        return assert(!"Invalid case.");
    }
}

// Adds new child to node.
// Finds place for new child (while keeping sorted order) and moves all children by 1 place to make
// space for new child. Inserts new child after that.
//
void Node4::add_child(const uint8_t key, entry_ptr child) noexcept
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

// Adds new child to node.
// Finds place for new child (while keeping sorted order) and moves all children by 1 place to make
// space for new child. Inserts new child after that.
//
void Node16::add_child(const uint8_t key, entry_ptr child) noexcept
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
    assert(!m_children[key]);

    m_children[key] = child;
    ++m_num_children;
}

void Node::remove_child(const uint8_t key) noexcept
{
    switch (m_type) {
    case Node4_t:
        return node4()->remove_child(key);
    case Node16_t:
        return node16()->remove_child(key);
    case Node48_t:
        return node48()->remove_child(key);
    case Node256_t:
        return node256()->remove_child(key);
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
    m_children[idx].reset();

    for (; idx < m_num_children - 1; ++idx) {
        m_keys[idx] = m_keys[idx + 1];
        m_children[idx] = m_children[idx + 1];
    }

    m_keys[idx] = 0;
    m_children[idx] = nullptr;

    --m_num_children;
}

void Node16::remove_child(const uint8_t key) noexcept
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

void Node48::remove_child(const uint8_t key) noexcept
{
    assert(m_idxs[key] != empty_slot);

    size_t idx = m_idxs[key];
    m_idxs[key] = empty_slot;
    m_children[idx].reset();

    --m_num_children;
}

void Node256::remove_child(const uint8_t key) noexcept
{
    assert(m_children[key]);

    m_children[key].reset();
    --m_num_children;
}

[[nodiscard]] bool Node::full() const noexcept
{
    switch (m_type) {
    case Node4_t:
        return node4()->full();
    case Node16_t:
        return node16()->full();
    case Node48_t:
        return node48()->full();
    case Node256_t:
        return node256()->full();
    default:
        return assert(!"Invalid case."), false;
    }
}

[[nodiscard]] bool Node::should_shrink() const noexcept
{
    switch (m_type) {
    case Node4_t:
        return node4()->should_shrink();
    case Node16_t:
        return node16()->should_shrink();
    case Node48_t:
        return node48()->should_shrink();
    case Node256_t:
        return node256()->should_shrink();
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
        return node4()->grow();
    case Node16_t:
        return node16()->grow();
    case Node48_t:
        return node48()->grow();
    default:
        return assert(!"Invalid case."), this;
    }
}

[[nodiscard]] Node16* Node4::grow() noexcept
{
    return new Node16{*this};
}

[[nodiscard]] Node48* Node16::grow() noexcept
{
    return new Node48{*this};
}

[[nodiscard]] Node256* Node48::grow() noexcept
{
    return new Node256{*this};
}

[[nodiscard]] Node* Node::shrink() noexcept
{
    switch (m_type) {
    case Node16_t:
        return node16()->shrink();
    case Node48_t:
        return node48()->shrink();
    case Node256_t:
        return node256()->shrink();
    default:
        return assert(!"Invalid case."), this;
    }
}

[[nodiscard]] Node4* Node16::shrink() noexcept
{
    return new Node4{*this};
}

[[nodiscard]] Node16* Node48::shrink() noexcept
{
    return new Node16{*this};
}

[[nodiscard]] Node48* Node256::shrink() noexcept
{
    return new Node48{*this};
}

[[nodiscard]] entry_ptr Node::collapse() noexcept
{
    switch (m_type) {
    case Node4_t:
        return node4()->collapse();
    default:
        return assert(!"Invalid case."), this;
    }
}

[[nodiscard]] const Leaf& Node::next_leaf() const noexcept
{
    switch (m_type) {
    case Node4_t:
        return node4()->next_leaf();
    case Node16_t:
        return node16()->next_leaf();
    case Node48_t:
        return node48()->next_leaf();
    case Node256_t:
        return node256()->next_leaf();
    default:
        return assert(!"Invalid case."), *Leaf::new_leaf(Key{nullptr, 0}, nullptr);
    }
}

// Returns next leaf from current entry. If current entry is leaf, it returns it. Otherwise, it
// calls next_leaf() on a node which will recurse down one level based on node type an call us
// again.
//
[[nodiscard]] constexpr const Leaf& entry_ptr::next_leaf() const noexcept
{
    assert(*this);
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
    return *Leaf::new_leaf(Key{nullptr, 0}, nullptr);
}

[[nodiscard]] const Leaf& Node256::next_leaf() const noexcept
{
    for (int i = 0; i < 256; ++i)
        if (m_children[i])
            return m_children[i].next_leaf();

    assert(false);
    return *Leaf::new_leaf(Key{nullptr, 0}, nullptr);
}

// Returns common prefix len for key at current depth and a prefix in our header.
// Header must not hold terminal byte, so we are comparing to minimal of node prefix len and max
// prefix len. If depth reaches key.size() - 1, we will get terminal byte and stop comparing.
//
[[nodiscard]] size_t Node::common_header_prefix(const Key& key, size_t depth) const noexcept
{
    const size_t max_cmp = hdrlen(m_prefix_len);
    size_t cp_len = 0;

    while (cp_len < max_cmp && key[depth + cp_len] == m_prefix[cp_len])
        ++cp_len;

    return cp_len;
}

// Returns common prefix length from this node to the leaf and a key at provided depth.
// If prefix in our header is max_prefix_len long and we've matched whole prefix, we need to find
// leaf to keep comparing. It is not important which leaf we take, because all of them have at least
// m_prefix_len common bytes.
//
[[nodiscard]] size_t Node::common_prefix(const Key& key, size_t depth) const noexcept
{
    size_t cp_len = common_header_prefix(key, depth);

    if (cp_len == max_prefix_len && cp_len < m_prefix_len) {
        const Leaf& leaf = next_leaf();
        while (cp_len < m_prefix_len && key[depth + cp_len] == leaf[depth + cp_len])
            ++cp_len;
    }

    return cp_len;
}

ART::~ART() noexcept
{
    destroy_all(m_root);
}

// Handles insertion when we reached leaf node. With current implementation, we will only insert new
// key if it differs from an exitisting leaf.
// TODO: Maybe implement substitution of the old entry.
//
void ART::insert_at_leaf(entry_ptr& entry, const Key& key, void* value, size_t depth) noexcept
{
    Leaf* leaf = entry.leaf_ptr();
    if (!leaf->match(key))
        entry = new Node4{key, value, depth, leaf};
}

// Handles insertion when we reached innder node and key at provided depth did not match full node
// prefix.
//
void ART::insert_at_node(entry_ptr& entry, const Key& key, void* value, size_t depth,
                         size_t cp_len) noexcept
{
    Node* node = entry.node_ptr();
    assert(cp_len != node->m_prefix_len);

    entry = new Node4{key, value, depth, node, cp_len};
}

// Insert new key into the tree.
//
void ART::insert(const Key& key, void* value) noexcept
{
    if (m_root)
        return insert(m_root, key, value, 0);

    m_root = Leaf::new_leaf(key, value);
}

// Inserts new key into the tree.
//
void ART::insert(entry_ptr& entry, const Key& key, void* value, size_t depth) noexcept
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

void ART::erase(const Key& key) noexcept
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
void ART::erase_leaf(entry_ptr& entry, Leaf* leaf, const Key& key, size_t depth) noexcept
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
void ART::erase(entry_ptr& entry, const Key& key, size_t depth) noexcept
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

    if (cp_len != hdrlen(node->m_prefix_len))
        return nullptr;

    depth += node->m_prefix_len;

    entry_ptr* next = node->find_child(key[depth]);
    if (next)
        return search(*next, key, depth + 1);

    return nullptr;
}

// Returns size of whole tree in bytes.
// By default, it include whole leafs. Leaf keys can be disabled by passing false.
//
size_t ART::size_in_bytes(bool full_leaves) const noexcept
{
    return sizeof(ART) + size_in_bytes(m_root, full_leaves);
}

size_t ART::size_in_bytes(const entry_ptr& entry, bool full_leaves) const noexcept
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

// Recursively visits all entries in a subtree and deletes them.
//
void ART::destroy_all(entry_ptr& entry)
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

// NOLINTEND
