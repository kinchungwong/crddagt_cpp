/**
 * @file opk_unique_list.hpp
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/opaque_ptr_key.hpp"

namespace crddagt
{

/**
 * @brief A container of unique `OpaquePtrKey<T>` elements with insertion-order preservation.
 *
 * @details
 * `OpkUniqueList<T>` stores a set of unique `OpaquePtrKey<T>` values while preserving the
 * order in which they were inserted. It provides O(1) average-case lookup by key and O(1)
 * access by index. Internally, it combines a `std::vector` (for ordered storage) with a
 * `std::unordered_map` (for key-to-index mapping).
 *
 * @par Construction
 * - Default constructible; starts empty with `size() == 0`.
 * - Copy constructible and copy assignable (deep copy; independent of original).
 * - Move constructible and move assignable.
 *
 * @par Null key rejection
 * - `insert()` throws `std::invalid_argument` if given a null `OpaquePtrKey<T>`.
 * - A key is null if constructed from `nullptr` or an expired `std::weak_ptr`.
 * - Consequence: no null keys are ever stored; all keys retrievable via `at()` are valid.
 *
 * @par Duplicate handling
 * - `insert()` returns the existing index if the key is already present.
 * - Duplicate insertions do not modify the list or change insertion order.
 *
 * @par Index semantics
 * - Indices are assigned sequentially starting from 0.
 * - The index returned by `insert()` for a new key equals the previous `size()`.
 * - `npos` (value: `SIZE_MAX`) represents "not found" in `find()` results.
 *
 * @par Invariants
 * - For all `i` in `[0, size())`: `find(at(i)) == i`.
 * - For any successfully inserted key `k`: `at(insert(k)) == k`.
 * - Elements are enumerated in insertion order.
 *
 * @par Exception safety
 * - `insert()` provides the strong exception guarantee: if it throws, the list is unchanged.
 * - `find()` and `size()` are `noexcept`.
 * - `at()` throws `std::out_of_range` for invalid indices.
 *
 * @par Thread safety
 * - No internal synchronization; not thread-safe.
 * - Concurrent reads (const operations) are safe.
 * - Concurrent read/write or write/write requires external synchronization.
 *
 * @par Callback reentrancy
 * - `enumerate()` does not protect against reentrancy.
 * - Modifying the list from within the `enumerate()` callback is undefined behavior.
 */
template <typename T>
class OpkUniqueList
{
public:
    /**
     * @brief Sentinel value indicating "not found" (equal to `SIZE_MAX`).
     */
    static constexpr std::size_t npos = ~static_cast<std::size_t>(0);

public:
    /**
     * @brief Insert an `OpaquePtrKey<T>` into the list if not already present.
     * @param opk The key to insert. Must not be null.
     * @return The index of the element: new index if inserted, existing index if duplicate.
     * @throw std::invalid_argument if `opk` is null (constructed from `nullptr` or expired
     *        `std::weak_ptr`).
     * @note Strong exception guarantee: if this function throws, the list is unchanged.
     * @note Complexity: O(1) average case.
     */
    std::size_t insert(const OpaquePtrKey<T>& opk)
    {
        if (!opk)
        {
            throw std::invalid_argument("OpkUniqueList::insert: null OpaquePtrKey");
        }
        auto it = m_map.find(opk);
        if (it != m_map.end())
        {
            return it->second;
        }
        std::size_t index = m_list.size();
        m_list.push_back(opk);
        m_map.emplace(opk, index);
        return index;
    }

    /**
     * @brief Find the index of the given `OpaquePtrKey<T>`.
     * @param opk The key to search for. May be null (will return `npos`).
     * @return The index if found; otherwise, `npos`.
     * @note Complexity: O(1) average case.
     */
    std::size_t find(const OpaquePtrKey<T>& opk) const noexcept
    {
        auto it = m_map.find(opk);
        if (it != m_map.end())
        {
            return it->second;
        }
        return npos;
    }

    /**
     * @brief Access the `OpaquePtrKey<T>` at the given index.
     * @param index The index to access. Must be in range `[0, size())`.
     * @return A copy of the `OpaquePtrKey<T>` at the specified index.
     * @throw std::out_of_range if `index >= size()`.
     * @note Complexity: O(1).
     */
    OpaquePtrKey<T> at(std::size_t index) const
    {
        if (index >= m_list.size())
        {
            throw std::out_of_range("OpkUniqueList::at: index out of range");
        }
        return m_list[index];
    }

    /**
     * @brief Return the number of elements in the list.
     * @return The count of unique keys that have been inserted.
     * @note Complexity: O(1).
     */
    std::size_t size() const noexcept
    {
        return m_list.size();
    }

    /**
     * @brief Enumerate all elements in insertion order.
     * @tparam Func A callable type with signature `void(std::size_t, const OpaquePtrKey<T>&)`.
     * @param func The callback to invoke for each element.
     * @note Complexity: O(n) where n is `size()`.
     * @warning Do not modify the list from inside the callback; behavior is undefined.
     */
    template <typename Func>
    void enumerate(Func&& func) const
    {
        static_assert(std::is_invocable_v<Func&, std::size_t, const OpaquePtrKey<T>&>,
            "Func must be callable as f(size_t, const OpaquePtrKey<T>&)");
        const size_t count = m_list.size();
        for (size_t idx = 0u; idx < count; ++idx)
        {
            func(idx, m_list[idx]);
        }
    }

private:
    std::vector<OpaquePtrKey<T>> m_list;
    std::unordered_map<OpaquePtrKey<T>, std::size_t> m_map;
};

} // namespace crddagt
