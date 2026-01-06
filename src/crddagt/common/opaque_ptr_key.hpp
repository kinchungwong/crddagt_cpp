/**
 * @file opaque_ptr_key.hpp
 */
#pragma once
#include "crddagt/common/common.hpp"

namespace crddagt
{

/**
 * @brief An opaque key representing a pointer to an object of type T.
 * @note Can be constructed from raw pointers, smart pointers, and weak pointers.
 * @note The key encapsulates a snapshot of the underlying raw pointer at the time of creation.
 * @note Type-parameterized; no operators are defined between different types of T.
 * @note If constructed from a weak pointer that has expired at the time of key creation, the key will be null.
 * @note Hashable, and is in fact designed to facilitate usage in hash-based containers.
 * @note Non-owning; does not manage the lifetime of the underlying object.
 * @note Correct usage requires the lifetime of the underlying object to outlive the key.
 * @warning Incorrect usage runs risk of false positives in case of underlying object being destroyed and its address reused.
 * @note Comparison operators compare the underlying pointer addresses after casting into `std::uintptr_t`.
 * @note Value-like semantics: can be trivially copied and assigned.
 * @note Thread semantics: acts like a built-in integer type; no internal synchronization.
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
