/**
 * @file iterable_union_find.hpp
 * @brief Class definition (member declaration) of the IterableUnionFind class template.
 * @note Source files that use member functions should include iterable_union_find.inline.hpp.
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/iterable_union_find.fwd.hpp"

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
 *         Supports uint16_t, uint32_t, or size_t.
 *         uint64_t is supported if exclusively targeting 64-bit platforms.
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
template <typename Idx = size_t>
class IterableUnionFind {
public:
    static_assert(std::is_unsigned_v<Idx>,
                  "IterableUnionFind: Idx must be an unsigned type");

    static_assert(sizeof(Idx) <= sizeof(size_t),
                  "IterableUnionFind: Idx size must not exceed size_t size");

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
     * @brief Reserves capacity for at least the specified number of elements, without creating.
     * This is a performance optimization to reduce reallocations.
     *
     * @param reserve_size The number of elements to reserve space for.
     *
     * @note If the reserve_size exceeds the maximum allowed elements, only up to the maximum
     * will be reserved.
     */
    void reserve(size_t reserve_size);

    /**
     * @brief Initializes the structure with a given number of singleton sets.
     *
     * @param count The number of singleton sets to initialize with.
     * @throws std::logic_error if called on a non-empty instance.
     */
    template <typename SizeType>
    void init_sets(SizeType count);

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
    [[nodiscard]] size_t class_size(Idx x) const;

    /**
     * @brief Returns the rank of the tree containing x.
     *
     * The rank is an upper bound on tree height, used for union-by-rank.
     * This is primarily for testing and debugging; the actual value is
     * an implementation detail and should not be relied upon.
     *
     * @param x An element in the class
     * @return The rank of the tree (stored at the root)
     * @throw std::runtime_error if x is out of range
     */
    [[nodiscard]] Idx class_rank(Idx x) const;

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

    // =========================================================================
    // Class enumeration
    // =========================================================================

    /**
     * @brief Returns the number of distinct equivalence classes.
     *
     * Counts elements that are roots (parent points to self).
     * This is an O(n) operation.
     *
     * @return The number of equivalence classes
     */
    [[nodiscard]] Idx num_classes() const;

    /**
     * @brief Populates a vector with the roots of all distinct equivalence classes.
     *
     * Each root uniquely identifies one equivalence class. The output vector
     * is cleared before populating. Roots are returned in index order.
     *
     * @param out_roots Output vector to populate with class roots
     *
     * @note Stability of representatives: after unions, the root of a class may
     *       change, according to union-by-rank rules. The exact rules are considered
     *       an implementation detail and should not be relied upon.
     */
    void get_class_representatives(std::vector<Idx>& out_roots) const;

    /**
     * @brief Populates a vector with all equivalence classes and their members.
     *
     * Convenience method equivalent to calling get_class_representatives()
     * followed by get_class_members() for each root.
     *
     * @param out_classes Output vector to populate with class member lists.
     *        Classes appear in root index order; members within each class
     *        appear in circular list traversal order starting from the root.
     */
    void get_classes(std::vector<std::vector<Idx>>& out_classes) const;

    // =========================================================================
    // Full state management
    // =========================================================================

    /**
     * @brief Exports a copy of the internal node data.
     *
     * Intended for inspection, testing, or serialization. The output is a
     * snapshot; modifications to the returned vector do not affect this instance.
     *
     * @param out Output vector to populate with node data. The vector is
     *        replaced (not appended to). Each node contains parent, rank,
     *        size, and next fields as documented in the Node struct.
     *
     * @note The internal representation is considered an implementation detail.
     *       Node field values (especially parent pointers after path compression)
     *       may vary between equivalent logical states.
     */
    void export_nodes(std::vector<Node>& out) const;

private:
    void init_sets_impl(Idx count);

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
