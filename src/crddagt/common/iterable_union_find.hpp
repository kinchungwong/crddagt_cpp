#ifndef CRDDAGT_COMMON_ITERABLE_UNION_FIND_HPP
#define CRDDAGT_COMMON_ITERABLE_UNION_FIND_HPP

#include "crddagt/common/iterable_union_find.fwd.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace crddagt {

/**
 * @brief A union-find data structure with O(class_size) iteration support.
 *
 * This class implements a disjoint-set (union-find) data structure with:
 * - **Union-by-rank**: Keeps trees balanced for O(alpha(n)) amortized find operations
 * - **Path compression**: Flattens trees during find for efficiency (two-pass iterative)
 * - **Exact size tracking**: Maintains class sizes with totality invariant
 * - **Circular linked list**: Enables O(class_size) enumeration of class members
 *
 * @tparam Idx The index type, defaults to size_t. Must be unsigned.
 *             Supports uint16_t, uint32_t, uint64_t, or size_t.
 *
 * @par Thread Safety
 * Externally synchronized. No internal synchronization. Caller must ensure:
 * - No concurrent modifications
 * - Concurrent const operations are safe only if no non-const operations occur
 *
 * @par Index Validation
 * All operations validate indices and throw std::runtime_error with a descriptive
 * message if an index is out of range. This prevents undefined behavior.
 *
 * @par Capacity
 * The maximum number of elements is limited by std::numeric_limits<Idx>::max().
 * make_set() throws std::overflow_error if this limit would be exceeded.
 *
 * @par Include Usage
 * - Include `iterable_union_find.hpp` for class definition (e.g., in headers with member variables)
 * - Include `iterable_union_find.inline.hpp` in source files that call member functions
 *
 * @remarks
 * `alpha(n)` is the inverse Ackermann function, which grows extremely slowly, and is
 * effectively a constant capped at 5 for all practical `n`.
 */
template <typename Idx>
class IterableUnionFind {
public:
    static_assert(std::is_unsigned_v<Idx>,
                  "IterableUnionFind: Idx must be an unsigned type");

    /**
     * @brief Per-element node storing union-find metadata.
     *
     * All fields use the same Idx type for uniformity and cache efficiency.
     */
    struct Node {
        Idx parent;  ///< Parent pointer (self if root)
        Idx rank;    ///< Tree rank for union-by-rank (bounded by log2(n))
        Idx size;    ///< Class size (valid only at root, 0 elsewhere)
        Idx next;    ///< Next element in circular linked list
    };

    // =========================================================================
    // Construction
    // =========================================================================

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
     * @throw std::overflow_error if adding another element would overflow Idx
     */
    Idx make_set();

    /**
     * @brief Returns the total number of elements created.
     *
     * @return The number of elements in the structure
     */
    [[nodiscard]] size_t element_count() const noexcept;

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
    Idx find(Idx x);

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
    bool unite(Idx a, Idx b);

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
    [[nodiscard]] Idx class_size(Idx x) const;

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
    [[nodiscard]] Idx class_root(Idx x) const;

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
    void get_class_members(Idx x, std::vector<Idx>& out) const;

    /**
     * @brief Checks if two elements are in the same equivalence class.
     *
     * @param a First element
     * @param b Second element
     * @return true if a and b are in the same class, false otherwise
     * @throw std::runtime_error if a or b is out of range
     */
    [[nodiscard]] bool same_class(Idx a, Idx b) const;

private:
    /**
     * @brief Validates that an index is within the valid range.
     *
     * @param x The index to validate
     * @throw std::runtime_error if x is out of range
     */
    void validate_index(Idx x) const;

    std::vector<Node> m_nodes;  ///< Per-element union-find metadata
};

} // namespace crddagt

#endif // CRDDAGT_COMMON_ITERABLE_UNION_FIND_HPP
