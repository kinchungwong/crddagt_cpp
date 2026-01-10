#ifndef CRDDAGT_COMMON_ITERABLE_UNION_FIND_HPP
#define CRDDAGT_COMMON_ITERABLE_UNION_FIND_HPP

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace crddagt {

/**
 * @brief A union-find data structure with O(class_size) iteration support.
 *
 * This class implements a disjoint-set (union-find) data structure with:
 * - **Union-by-rank**: Keeps trees balanced for O(α(n)) amortized find operations
 * - **Path compression**: Flattens trees during find for efficiency (two-pass iterative)
 * - **Exact size tracking**: Maintains class sizes with totality invariant
 * - **Circular linked list**: Enables O(class_size) enumeration of class members
 *
 * @tparam Idx The index type, defaults to size_t
 *
 * @par Thread Safety
 * Externally synchronized. No internal synchronization. Caller must ensure:
 * - No concurrent modifications
 * - Concurrent const operations are safe only if no non-const operations occur
 *
 * @par Index Validation
 * All operations validate indices and throw std::runtime_error with a descriptive
 * message if an index is out of range. This prevents undefined behavior.
 */
template <typename Idx = size_t>
class IterableUnionFind {
public:
    static_assert(std::is_unsigned_v<Idx>,
                  "IterableUnionFind: Idx must be an unsigned type");

    /**
     * @brief Default constructor creates an empty union-find structure.
     */
    IterableUnionFind() = default;

    // =========================================================================
    // Element Management
    // =========================================================================

    /**
     * @brief Creates a new singleton set and returns its index.
     *
     * Indices are assigned sequentially starting from 0.
     *
     * @return The index of the newly created element
     */
    Idx make_set() {
        Idx x = static_cast<Idx>(m_parent.size());
        m_parent.push_back(x);      // Self-parent (is own root)
        m_rank.push_back(0);        // Initial rank 0
        m_size.push_back(1);        // Singleton has size 1
        m_next.push_back(x);        // Self-loop (singleton circular list)
        return x;
    }

    /**
     * @brief Returns the total number of elements created.
     *
     * @return The number of elements in the structure
     */
    [[nodiscard]] size_t element_count() const noexcept {
        return m_parent.size();
    }

    // =========================================================================
    // Core Operations
    // =========================================================================

    /**
     * @brief Finds the root of the set containing x, with path compression.
     *
     * Uses iterative two-pass path compression:
     * - Pass 1: Traverse from x to root
     * - Pass 2: Traverse again, rewriting each node's parent to point to root
     *
     * @param x The element to find the root of
     * @return The root of the set containing x
     * @throw std::runtime_error if x is out of range
     *
     * @note This method is non-const because path compression modifies state.
     *       Use class_root() for const access (without compression).
     */
    Idx find(Idx x) {
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

    /**
     * @brief Merges the sets containing a and b.
     *
     * Uses union-by-rank to keep trees balanced. The circular linked lists
     * are spliced at the roots for deterministic behavior.
     *
     * @param a An element in the first set
     * @param b An element in the second set
     * @return true if a merge occurred, false if a and b were already in the same set
     * @throw std::runtime_error if a or b is out of range
     */
    bool unite(Idx a, Idx b) {
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

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * @brief Returns the size of the equivalence class containing x.
     *
     * @param x An element in the class
     * @return The number of elements in the class containing x
     * @throw std::runtime_error if x is out of range
     */
    [[nodiscard]] size_t class_size(Idx x) const {
        return m_size[class_root(x)];
    }

    /**
     * @brief Finds the root of the set containing x, without path compression.
     *
     * This is a const method that does not modify the structure.
     * Use find() for better amortized performance when const is not required.
     *
     * @param x The element to find the root of
     * @return The root of the set containing x
     * @throw std::runtime_error if x is out of range
     */
    [[nodiscard]] Idx class_root(Idx x) const {
        validate_index(x);
        while (m_parent[x] != x) {
            x = m_parent[x];
        }
        return x;
    }

    /**
     * @brief Populates a vector with all members of the equivalence class containing x.
     *
     * The output vector is cleared before populating. Caller can reserve
     * capacity with class_size(x) to eliminate reallocations.
     *
     * @param x An element in the class
     * @param out Output vector to populate with class members
     * @throw std::runtime_error if x is out of range
     */
    void get_class_members(Idx x, std::vector<Idx>& out) const {
        validate_index(x);
        out.clear();
        Idx current = x;
        do {
            out.push_back(current);
            current = m_next[current];
        } while (current != x);
    }

    /**
     * @brief Checks if two elements are in the same equivalence class.
     *
     * @param a First element
     * @param b Second element
     * @return true if a and b are in the same class, false otherwise
     * @throw std::runtime_error if a or b is out of range
     */
    [[nodiscard]] bool same_class(Idx a, Idx b) const {
        return class_root(a) == class_root(b);
    }

private:
    /**
     * @brief Validates that an index is within the valid range.
     *
     * @param x The index to validate
     * @throw std::runtime_error if x is out of range
     */
    void validate_index(Idx x) const {
        if (static_cast<size_t>(x) >= m_parent.size()) {
            throw std::runtime_error(
                "IterableUnionFind: index " + std::to_string(x) +
                " out of range [0, " + std::to_string(m_parent.size()) + ")");
        }
    }

    std::vector<Idx> m_parent;    // Parent pointer (root if m_parent[x] == x)
    std::vector<size_t> m_rank;   // Tree rank for union-by-rank
    std::vector<size_t> m_size;   // Class size (valid only at root, 0 elsewhere)
    std::vector<Idx> m_next;      // Circular linked list for iteration
};

} // namespace crddagt

#endif // CRDDAGT_COMMON_ITERABLE_UNION_FIND_HPP
