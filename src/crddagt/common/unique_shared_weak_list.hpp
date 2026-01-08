/**
 * @file unique_shared_weak_list.hpp
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/opaque_ptr_key.hpp"

namespace crddagt
{

/**
 * @brief A container of unique pointers with controllable strong/weak storage.
 *
 * @details
 * `UniqueSharedWeakList<T>` stores pointers to objects of type `T`, identified by their
 * address (using `OpaquePtrKey<T>`). Each entry can be stored as either a strong reference
 * (`std::shared_ptr<T>`) or a weak reference (`std::weak_ptr<T>`). Items are inserted as
 * strong references by default. The `weaken()` method converts an entry to weak storage,
 * allowing the object to be destroyed externally. The `strengthen()` method converts back
 * to strong storage if the object is still alive.
 *
 * @par Construction
 * - Default constructible; starts empty with `size() == 0`.
 * - Copy constructible and copy assignable (deep copy; independent of original).
 * - Move constructible and move assignable.
 *
 * @par Insertion
 * - `insert(shared_ptr)` and `insert(weak_ptr)` both store as strong references initially.
 * - Throws `std::invalid_argument` if null or expired.
 * - Duplicate insertions (same address) return the existing index without modification.
 * - The storage mode (strong/weak) is not changed by duplicate insertion.
 *
 * @par Strong vs Weak Storage
 * - Strong storage: the list holds a `shared_ptr`, preventing object destruction.
 * - Weak storage: the list holds a `weak_ptr`, allowing external destruction.
 * - Use `weaken(index)` to convert strong to weak.
 * - Use `strengthen(index)` to convert weak to strong (throws if expired).
 * - Use `is_strong(index)` to query the current storage mode.
 * - Use `is_expired(index)` to check if a weak entry has expired.
 *
 * @par Access
 * - `at(index)` returns `shared_ptr<T>`, throws if index invalid or item expired.
 * - `get(index)` returns `shared_ptr<T>` or nullptr if expired (no throw for expiration).
 * - `find(ptr)` returns the index or `npos`; noexcept, works even for expired entries.
 *
 * @par Key Permanence
 * - Once inserted, an entry's `OpaquePtrKey<T>` (derived from the pointer address) is
 *   stored permanently and never changes, even if the entry is weakened and expires.
 * - This means `find()` can locate an entry even after expiration.
 *
 * @par Invariants
 * - No null or expired pointers are inserted.
 * - Entries are unique by pointer address.
 * - Insertion order is preserved.
 * - Strong entries never expire while in the list.
 * - For non-expired entry at index `i`: `find(at(i).get()) == i`.
 *
 * @par Exception Safety
 * - `insert()` provides strong exception guarantee.
 * - `weaken()` and `strengthen()` throw for invalid index or (strengthen only) expiration.
 * - `at()` throws for invalid index or expiration.
 * - `get()`, `find()`, `size()`, `is_strong()`, `is_expired()` do not throw for expiration.
 *
 * @par Thread Safety
 * - No internal synchronization; not thread-safe.
 * - Concurrent reads are safe if no entry is being weakened/strengthened.
 * - External synchronization required for any concurrent modification.
 *
 * @par Callback Reentrancy
 * - `enumerate()` does not protect against reentrancy.
 * - Modifying the list from within the callback is undefined behavior.
 */
template <typename T>
class UniqueSharedWeakList
{
public:
    /**
     * @brief Sentinel value indicating "not found" (equal to `SIZE_MAX`).
     */
    static constexpr std::size_t npos = ~static_cast<std::size_t>(0);

    /**
     * @brief Exception thrown when accessing an expired weak entry.
     */
    class expired_entry_error : public std::runtime_error
    {
    public:
        explicit expired_entry_error(const std::string& msg)
            : std::runtime_error(msg)
        {
        }
    };

public:
    /**
     * @brief Insert a `shared_ptr<T>` into the list (stored as strong reference).
     * @param ptr The shared pointer to insert. Must not be null.
     * @return The index of the element: new index if inserted, existing index if duplicate.
     * @throw std::invalid_argument if `ptr` is null.
     * @note Strong exception guarantee.
     * @note Complexity: O(1) average case.
     */
    std::size_t insert(const std::shared_ptr<T>& ptr)
    {
        if (!ptr)
        {
            throw std::invalid_argument(
                "UniqueSharedWeakList::insert: null shared_ptr");
        }
        return insert_impl(ptr);
    }

    /**
     * @brief Insert a `shared_ptr<const T>` into the list (stored as strong reference).
     * @param ptr The shared pointer to insert. Must not be null.
     * @return The index of the element: new index if inserted, existing index if duplicate.
     * @throw std::invalid_argument if `ptr` is null.
     * @note Strong exception guarantee.
     * @note Complexity: O(1) average case.
     */
    std::size_t insert(const std::shared_ptr<const T>& ptr)
    {
        if (!ptr)
        {
            throw std::invalid_argument(
                "UniqueSharedWeakList::insert: null shared_ptr<const T>");
        }
        return insert_impl(std::const_pointer_cast<T>(ptr));
    }

    /**
     * @brief Insert from a `weak_ptr<T>` (must not be expired, stored as strong reference).
     * @param ptr The weak pointer to insert. Must not be expired.
     * @return The index of the element: new index if inserted, existing index if duplicate.
     * @throw std::invalid_argument if `ptr` is expired.
     * @note Strong exception guarantee.
     * @note Complexity: O(1) average case.
     */
    std::size_t insert(std::weak_ptr<T> ptr)
    {
        auto locked = ptr.lock();
        if (!locked)
        {
            throw std::invalid_argument(
                "UniqueSharedWeakList::insert: expired weak_ptr");
        }
        return insert_impl(locked);
    }

    /**
     * @brief Insert from a `weak_ptr<const T>` (must not be expired, stored as strong).
     * @param ptr The weak pointer to insert. Must not be expired.
     * @return The index of the element: new index if inserted, existing index if duplicate.
     * @throw std::invalid_argument if `ptr` is expired.
     * @note Strong exception guarantee.
     * @note Complexity: O(1) average case.
     */
    std::size_t insert(std::weak_ptr<const T> ptr)
    {
        auto locked = ptr.lock();
        if (!locked)
        {
            throw std::invalid_argument(
                "UniqueSharedWeakList::insert: expired weak_ptr<const T>");
        }
        return insert_impl(std::const_pointer_cast<T>(locked));
    }

    /**
     * @brief Convert the entry at index to weak storage.
     * @param index The index to weaken. Must be in range `[0, size())`.
     * @throw std::out_of_range if `index >= size()`.
     * @note If already weak, this is a no-op.
     * @note Complexity: O(1).
     */
    void weaken(std::size_t index)
    {
        if (index >= m_entries.size())
        {
            throw std::out_of_range("UniqueSharedWeakList::weaken: index out of range");
        }
        auto& entry = m_entries[index];
        if (std::holds_alternative<std::shared_ptr<T>>(entry.storage))
        {
            auto& sp = std::get<std::shared_ptr<T>>(entry.storage);
            std::weak_ptr<T> wp = sp;
            entry.storage = wp;
        }
    }

    /**
     * @brief Convert the entry at index to strong storage.
     * @param index The index to strengthen. Must be in range `[0, size())`.
     * @throw std::out_of_range if `index >= size()`.
     * @throw expired_entry_error if the entry is weak and has expired.
     * @note If already strong, this is a no-op.
     * @note Complexity: O(1).
     */
    void strengthen(std::size_t index)
    {
        if (index >= m_entries.size())
        {
            throw std::out_of_range(
                "UniqueSharedWeakList::strengthen: index out of range");
        }
        auto& entry = m_entries[index];
        if (std::holds_alternative<std::weak_ptr<T>>(entry.storage))
        {
            auto& wp = std::get<std::weak_ptr<T>>(entry.storage);
            auto sp = wp.lock();
            if (!sp)
            {
                throw expired_entry_error(
                    "UniqueSharedWeakList::strengthen: entry has expired");
            }
            entry.storage = sp;
        }
    }

    /**
     * @brief Access the `shared_ptr<T>` at the given index.
     * @param index The index to access. Must be in range `[0, size())`.
     * @return The `shared_ptr<T>` at the specified index.
     * @throw std::out_of_range if `index >= size()`.
     * @throw expired_entry_error if the entry is weak and has expired.
     * @note Complexity: O(1).
     */
    std::shared_ptr<T> at(std::size_t index) const
    {
        if (index >= m_entries.size())
        {
            throw std::out_of_range("UniqueSharedWeakList::at: index out of range");
        }
        const auto& entry = m_entries[index];
        if (std::holds_alternative<std::shared_ptr<T>>(entry.storage))
        {
            return std::get<std::shared_ptr<T>>(entry.storage);
        }
        else
        {
            auto sp = std::get<std::weak_ptr<T>>(entry.storage).lock();
            if (!sp)
            {
                throw expired_entry_error(
                    "UniqueSharedWeakList::at: entry has expired");
            }
            return sp;
        }
    }

    /**
     * @brief Access the `shared_ptr<T>` at the given index, returning nullptr if expired.
     * @param index The index to access. Must be in range `[0, size())`.
     * @return The `shared_ptr<T>` at the index, or nullptr if weak and expired.
     * @throw std::out_of_range if `index >= size()`.
     * @note Complexity: O(1).
     */
    std::shared_ptr<T> get(std::size_t index) const
    {
        if (index >= m_entries.size())
        {
            throw std::out_of_range("UniqueSharedWeakList::get: index out of range");
        }
        const auto& entry = m_entries[index];
        if (std::holds_alternative<std::shared_ptr<T>>(entry.storage))
        {
            return std::get<std::shared_ptr<T>>(entry.storage);
        }
        else
        {
            return std::get<std::weak_ptr<T>>(entry.storage).lock();
        }
    }

    /**
     * @brief Find the index of the given pointer.
     * @param ptr The raw pointer to search for.
     * @return The index if found; otherwise, `npos`.
     * @note Works even for expired weak entries (key is still stored).
     * @note Complexity: O(1) average case.
     */
    std::size_t find(const T* ptr) const noexcept
    {
        if (!ptr)
        {
            return npos;
        }
        OpaquePtrKey<T> key(ptr);
        auto it = m_map.find(key);
        if (it != m_map.end())
        {
            return it->second;
        }
        return npos;
    }

    /**
     * @brief Find the index of the given shared_ptr.
     * @param ptr The shared pointer to search for.
     * @return The index if found; otherwise, `npos`.
     * @note Complexity: O(1) average case.
     */
    std::size_t find(const std::shared_ptr<T>& ptr) const noexcept
    {
        return find(ptr.get());
    }

    /**
     * @brief Find the index of the given shared_ptr<const T>.
     * @param ptr The shared pointer to search for.
     * @return The index if found; otherwise, `npos`.
     * @note Complexity: O(1) average case.
     */
    std::size_t find(const std::shared_ptr<const T>& ptr) const noexcept
    {
        return find(ptr.get());
    }

    /**
     * @brief Find the index of the given weak_ptr (if not expired).
     * @param ptr The weak pointer to search for.
     * @return The index if found and not expired; otherwise, `npos`.
     * @note Returns `npos` if the weak_ptr is expired.
     * @note Complexity: O(1) average case.
     */
    std::size_t find(const std::weak_ptr<T>& ptr) const noexcept
    {
        auto locked = ptr.lock();
        return find(locked.get());
    }

    /**
     * @brief Check if the entry at index is stored as a strong reference.
     * @param index The index to check. Must be in range `[0, size())`.
     * @return `true` if strong, `false` if weak.
     * @throw std::out_of_range if `index >= size()`.
     * @note Complexity: O(1).
     */
    bool is_strong(std::size_t index) const
    {
        if (index >= m_entries.size())
        {
            throw std::out_of_range(
                "UniqueSharedWeakList::is_strong: index out of range");
        }
        return std::holds_alternative<std::shared_ptr<T>>(m_entries[index].storage);
    }

    /**
     * @brief Check if the entry at index has expired.
     * @param index The index to check. Must be in range `[0, size())`.
     * @return `true` if weak and expired, `false` otherwise.
     * @throw std::out_of_range if `index >= size()`.
     * @note Strong entries always return `false`.
     * @note Complexity: O(1).
     */
    bool is_expired(std::size_t index) const
    {
        if (index >= m_entries.size())
        {
            throw std::out_of_range(
                "UniqueSharedWeakList::is_expired: index out of range");
        }
        const auto& entry = m_entries[index];
        if (std::holds_alternative<std::weak_ptr<T>>(entry.storage))
        {
            return std::get<std::weak_ptr<T>>(entry.storage).expired();
        }
        return false;
    }

    /**
     * @brief Return the number of entries in the list.
     * @return The count of entries (including expired weak entries).
     * @note Complexity: O(1).
     */
    std::size_t size() const noexcept
    {
        return m_entries.size();
    }

    /**
     * @brief Enumerate all entries in insertion order.
     * @tparam Func A callable type with signature
     *         `void(std::size_t, std::shared_ptr<T>, bool is_strong, bool is_expired)`.
     * @param func The callback to invoke for each entry.
     * @note For expired weak entries, the shared_ptr will be nullptr.
     * @note Complexity: O(n) where n is `size()`.
     * @warning Do not modify the list from inside the callback; behavior is undefined.
     */
    template <typename Func>
    void enumerate(Func&& func) const
    {
        static_assert(
            std::is_invocable_v<Func&, std::size_t, std::shared_ptr<T>, bool, bool>,
            "Func must be callable as f(size_t, shared_ptr<T>, bool is_strong, bool is_expired)");
        const std::size_t count = m_entries.size();
        for (std::size_t idx = 0u; idx < count; ++idx)
        {
            const auto& entry = m_entries[idx];
            bool strong = std::holds_alternative<std::shared_ptr<T>>(entry.storage);
            std::shared_ptr<T> ptr;
            bool expired = false;
            if (strong)
            {
                ptr = std::get<std::shared_ptr<T>>(entry.storage);
            }
            else
            {
                ptr = std::get<std::weak_ptr<T>>(entry.storage).lock();
                expired = !ptr;
            }
            func(idx, ptr, strong, expired);
        }
    }

    /**
     * @brief Get the stored key at the given index.
     * @param index The index to access. Must be in range `[0, size())`.
     * @return The `OpaquePtrKey<T>` for the entry.
     * @throw std::out_of_range if `index >= size()`.
     * @note The key remains valid even for expired weak entries.
     * @note Complexity: O(1).
     */
    OpaquePtrKey<T> key_at(std::size_t index) const
    {
        if (index >= m_entries.size())
        {
            throw std::out_of_range(
                "UniqueSharedWeakList::key_at: index out of range");
        }
        return m_entries[index].key;
    }

private:
    std::size_t insert_impl(const std::shared_ptr<T>& ptr)
    {
        OpaquePtrKey<T> key(ptr);
        auto it = m_map.find(key);
        if (it != m_map.end())
        {
            return it->second;
        }
        std::size_t index = m_entries.size();
        m_entries.push_back(Entry{key, ptr});
        m_map.emplace(key, index);
        return index;
    }

private:
    struct Entry
    {
        OpaquePtrKey<T> key;
        std::variant<std::shared_ptr<T>, std::weak_ptr<T>> storage;

        Entry(OpaquePtrKey<T> k, std::shared_ptr<T> sp)
            : key(k)
            , storage(std::move(sp))
        {
        }
    };

    std::vector<Entry> m_entries;
    std::unordered_map<OpaquePtrKey<T>, std::size_t> m_map;
};

} // namespace crddagt
