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
    Idx x = static_cast<Idx>(m_parent.size());
    m_parent.push_back(x);      // Self-parent (is own root)
    m_rank.push_back(0);        // Initial rank 0
    m_size.push_back(1);        // Singleton has size 1
    m_next.push_back(x);        // Self-loop (singleton circular list)
    return x;
}

template <typename Idx>
size_t IterableUnionFind<Idx>::element_count() const noexcept
{
    return m_parent.size();
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
    while (m_parent[root] != root) {
        root = m_parent[root];
    }

    // Pass 2: Path compression - rewrite parents to point to root
    while (m_parent[x] != root) {
        Idx next = m_parent[x];
        m_parent[x] = root;
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
    size_t combined_size = m_size[root_a] + m_size[root_b];

    // Union by rank
    Idx new_root, old_root;
    if (m_rank[root_a] < m_rank[root_b]) {
        m_parent[root_a] = root_b;
        new_root = root_b;
        old_root = root_a;
    } else if (m_rank[root_a] > m_rank[root_b]) {
        m_parent[root_b] = root_a;
        new_root = root_a;
        old_root = root_b;
    } else {
        m_parent[root_b] = root_a;
        m_rank[root_a]++;
        new_root = root_a;
        old_root = root_b;
    }

    // Update sizes
    m_size[new_root] = combined_size;
    m_size[old_root] = 0;

    // Splice circular lists at the roots for deterministic behavior
    std::swap(m_next[root_a], m_next[root_b]);

    return true;
}

// =============================================================================
// Queries
// =============================================================================

template <typename Idx>
size_t IterableUnionFind<Idx>::class_size(Idx x) const
{
    return m_size[class_root(x)];
}

template <typename Idx>
Idx IterableUnionFind<Idx>::class_root(Idx x) const
{
    validate_index(x);
    while (m_parent[x] != x) {
        x = m_parent[x];
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
        current = m_next[current];
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
    if (static_cast<size_t>(x) >= m_parent.size()) {
        throw std::runtime_error(
            "IterableUnionFind: index " + std::to_string(x) +
            " out of range [0, " + std::to_string(m_parent.size()) + ")");
    }
}

} // namespace crddagt

#endif // CRDDAGT_COMMON_ITERABLE_UNION_FIND_INLINE_HPP
