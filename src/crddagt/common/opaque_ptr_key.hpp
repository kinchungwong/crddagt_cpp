/**
 * @file opaque_ptr_key.hpp
 */
#pragma once
#include "crddagt/common/common.hpp"

namespace crddagt
{

/**
 * @brief A non-dereferenceable, hashable identifier derived from a pointer address.
 *
 * @details
 * OpaquePtrKey captures a pointer's address as a `std::uintptr_t` at construction time,
 * enabling use as a key in hash-based and ordered containers. The original pointer
 * cannot be recovered; the key serves only for identity comparison and hashing.
 *
 * @par Construction
 * - From `const T*` (raw pointer)
 * - From `std::unique_ptr<T>` or `std::unique_ptr<const T>`
 * - From `std::shared_ptr<T>` or `std::shared_ptr<const T>`
 * - From `std::weak_ptr<T>` or `std::weak_ptr<const T>` (captures current lock state)
 * - No default constructor; must be initialized from a pointer source.
 * - If constructed from an expired `std::weak_ptr`, the key is null.
 *
 * @par Null state
 * - A key is null if constructed from `nullptr` or an expired `std::weak_ptr`.
 * - Test with `!key` (returns true if null). No `operator bool()` is provided.
 *
 * @par Type safety
 * - Template parameter `T` provides type-level separation.
 * - Comparison operators only accept `OpaquePtrKey<T>` with the same `T`.
 * - Hash values incorporate `typeid(T)`, so keys from different `T` hash differently
 *   even if derived from the same address.
 *
 * @par Ownership and lifetime
 * - Non-owning: does not prevent destruction of the pointed-to object.
 * - The key remains valid (as a numeric value) after the object is destroyed.
 * - Constraint: the key should not be used for identity lookup after the object's
 *   destruction, because address reuse may cause a different object to compare equal
 *   (identity collision).
 * - Recommended safeguard: containers should consider owning the objects via
 *   `std::shared_ptr` so that the keys remain valid and unique.
 *
 * @par Value semantics
 * - Trivially copyable and assignable.
 * - All comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`) compare the stored
 *   `std::uintptr_t` values directly.
 *
 * @par Thread safety
 * - Equivalent to a `std::uintptr_t`: no internal synchronization.
 * - Concurrent reads are safe; concurrent read/write requires external synchronization.
 *
 * @par Standard library integration
 * - `std::hash<OpaquePtrKey<T>>` specialization provided.
 */
template <typename T>
class OpaquePtrKey
{
public:
    explicit OpaquePtrKey(const T* ptr) noexcept
        : m_value(reinterpret_cast<std::uintptr_t>(ptr))
    {
    }

    explicit OpaquePtrKey(const std::unique_ptr<T>& ptr) noexcept
        : m_value(reinterpret_cast<std::uintptr_t>(ptr.get()))
    {
    }

    explicit OpaquePtrKey(const std::unique_ptr<const T>& ptr) noexcept
        : m_value(reinterpret_cast<std::uintptr_t>(ptr.get()))
    {
    }

    explicit OpaquePtrKey(const std::shared_ptr<T>& ptr) noexcept
        : m_value(reinterpret_cast<std::uintptr_t>(ptr.get()))
    {
    }

    explicit OpaquePtrKey(const std::shared_ptr<const T>& ptr) noexcept
        : m_value(reinterpret_cast<std::uintptr_t>(ptr.get()))
    {
    }

    explicit OpaquePtrKey(std::weak_ptr<T> ptr) noexcept
        : m_value(reinterpret_cast<std::uintptr_t>(ptr.lock().get()))
    {
    }

    explicit OpaquePtrKey(std::weak_ptr<const T> ptr) noexcept
        : m_value(reinterpret_cast<std::uintptr_t>(ptr.lock().get()))
    {
    }

    bool operator==(const OpaquePtrKey<T>& other) const noexcept
    {
        return m_value == other.m_value;
    }

    bool operator!=(const OpaquePtrKey<T>& other) const noexcept
    {
        return !(*this == other);
    }

    bool operator>(const OpaquePtrKey<T>& other) const noexcept
    {
        return m_value > other.m_value;
    }

    bool operator<(const OpaquePtrKey<T>& other) const noexcept
    {
        return m_value < other.m_value;
    }

    bool operator>=(const OpaquePtrKey<T>& other) const noexcept
    {
        return !(*this < other);
    }

    bool operator<=(const OpaquePtrKey<T>& other) const noexcept
    {
        return !(*this > other);
    }

    bool operator!() const noexcept
    {
        return !m_value;
    }

    std::size_t hash() const noexcept
    {
        return std::hash<std::uintptr_t>()(m_value) ^ stc_type_hash();
    }

private:
    static std::size_t stc_type_hash() noexcept
    {
        return std::type_index(typeid(T)).hash_code();
    }

private:
    std::uintptr_t m_value;
};

} // namespace crddagt

namespace std
{

template<typename T>
struct hash<crddagt::OpaquePtrKey<T>>
{
    std::size_t operator()(const crddagt::OpaquePtrKey<T>& key) const noexcept
    {
        return key.hash();
    }
};

} // namespace std
