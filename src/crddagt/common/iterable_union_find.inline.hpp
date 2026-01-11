#ifndef CRDDAGT_COMMON_ITERABLE_UNION_FIND_INLINE_HPP
#define CRDDAGT_COMMON_ITERABLE_UNION_FIND_INLINE_HPP

#include "crddagt/common/iterable_union_find.hpp"

namespace crddagt {

// =============================================================================
// Element Management
// =============================================================================

template <typename Idx>
Idx IterableUnionFind<Idx>::make_set()
{
    // Overflow check: ensure the new index fits in Idx
    if (m_nodes.size() >= static_cast<size_t>(std::numeric_limits<Idx>::max())) {
        throw std::overflow_error(
            "IterableUnionFind: cannot create more than " +
            std::to_string(std::numeric_limits<Idx>::max()) + " elements");
    }

    Idx x = static_cast<Idx>(m_nodes.size());
    m_nodes.push_back(Node{
        x,      // parent: self (is own root)
        0,      // rank: initial 0
        1,      // size: singleton has size 1
        x       // next: self-loop (singleton circular list)
    });
    return x;
}

template <typename Idx>
size_t IterableUnionFind<Idx>::element_count() const noexcept
{
    return m_nodes.size();
}

// =============================================================================
// Core Operations
// =============================================================================

template <typename Idx>
Idx IterableUnionFind<Idx>::find(Idx x)
{
    validate_index(x);

    // Pass 1: Find root
    Idx root = x;
    while (m_nodes[root].parent != root) {
        root = m_nodes[root].parent;
    }

    // Pass 2: Path compression - rewrite parents to point to root
    while (m_nodes[x].parent != root) {
        Idx next = m_nodes[x].parent;
        m_nodes[x].parent = root;
        x = next;
    }

    return root;
}

template <typename Idx>
bool IterableUnionFind<Idx>::unite(Idx a, Idx b)
{
    // validate_index called by find()
    Idx root_a = find(a);
    Idx root_b = find(b);

    if (root_a == root_b) {
        return false;  // Already in same class
    }

    // Compute combined size before modifying
    // Note: combined_size cannot overflow because total size <= element_count,
    // and element_count is bounded by Idx::max (enforced by make_set).
    Idx combined_size = m_nodes[root_a].size + m_nodes[root_b].size;

    // Union by rank
    Idx new_root, old_root;
    if (m_nodes[root_a].rank < m_nodes[root_b].rank) {
        m_nodes[root_a].parent = root_b;
        new_root = root_b;
        old_root = root_a;
    } else if (m_nodes[root_a].rank > m_nodes[root_b].rank) {
        m_nodes[root_b].parent = root_a;
        new_root = root_a;
        old_root = root_b;
    } else {
        m_nodes[root_b].parent = root_a;
        // Rank increment is safe: rank <= log2(n) < sizeof(Idx)*8
        ++m_nodes[root_a].rank;
        new_root = root_a;
        old_root = root_b;
    }

    // Update sizes
    m_nodes[new_root].size = combined_size;
    m_nodes[old_root].size = 0;

    // Splice circular lists at the roots for deterministic behavior
    std::swap(m_nodes[root_a].next, m_nodes[root_b].next);

    return true;
}

// =============================================================================
// Queries
// =============================================================================

template <typename Idx>
Idx IterableUnionFind<Idx>::class_size(Idx x) const
{
    return m_nodes[class_root(x)].size;
}

template <typename Idx>
Idx IterableUnionFind<Idx>::class_root(Idx x) const
{
    validate_index(x);
    while (m_nodes[x].parent != x) {
        x = m_nodes[x].parent;
    }
    return x;
}

template <typename Idx>
void IterableUnionFind<Idx>::get_class_members(Idx x, std::vector<Idx>& out) const
{
    validate_index(x);
    out.clear();
    Idx current = x;
    do {
        out.push_back(current);
        current = m_nodes[current].next;
    } while (current != x);
}

template <typename Idx>
bool IterableUnionFind<Idx>::same_class(Idx a, Idx b) const
{
    return class_root(a) == class_root(b);
}

// =============================================================================
// Private Helpers
// =============================================================================

template <typename Idx>
void IterableUnionFind<Idx>::validate_index(Idx x) const
{
    if (static_cast<size_t>(x) >= m_nodes.size()) {
        throw std::runtime_error(
            "IterableUnionFind: index " + std::to_string(x) +
            " out of range [0, " + std::to_string(m_nodes.size()) + ")");
    }
}

} // namespace crddagt

#endif // CRDDAGT_COMMON_ITERABLE_UNION_FIND_INLINE_HPP
