/**
 * @file vardata.inline.hpp
 * @brief Implementations for type-parameterized member methods in the VarData class.
 */
#pragma once
#include "crddagt/common/vardata.hpp"

namespace crddagt
{

namespace detail
{

/**
 * @brief Type trait to validate VarData template parameter.
 * @details Ensures T is not void, reference, array, or cv-qualified.
 */
template <typename T>
struct is_valid_vardata_type
{
    using Decayed = std::decay_t<T>;
    static constexpr bool value =
        !std::is_void_v<Decayed> &&
        !std::is_reference_v<T> &&
        !std::is_array_v<Decayed> &&
        std::is_same_v<T, Decayed>;
};

template <typename T>
inline constexpr bool is_valid_vardata_type_v = is_valid_vardata_type<T>::value;

/**
 * @brief Helper to get the decayed storage type.
 */
template <typename T>
using storage_type_t = std::decay_t<T>;

} // namespace detail

template <typename T>
bool VarData::has_type() const noexcept
{
    using StorageT = detail::storage_type_t<T>;
    static_assert(!std::is_void_v<StorageT>, "VarData: T cannot be void");
    return m_ti == std::type_index{typeid(StorageT)};
}

template <typename T, typename... Args>
void VarData::emplace(Args&&... args)
{
    using StorageT = detail::storage_type_t<T>;
    static_assert(!std::is_void_v<StorageT>, "VarData: T cannot be void");
    static_assert(!std::is_array_v<StorageT>, "VarData: T cannot be an array type");

    auto ptr = std::make_shared<StorageT>(std::forward<Args>(args)...);
    m_pvoid = std::static_pointer_cast<void>(ptr);
    m_ti = std::type_index{typeid(StorageT)};
}

template <typename T>
void VarData::set(T&& value)
{
    using StorageT = detail::storage_type_t<T>;
    static_assert(!std::is_void_v<StorageT>, "VarData: T cannot be void");
    static_assert(!std::is_array_v<StorageT>, "VarData: T cannot be an array type");

    auto ptr = std::make_shared<StorageT>(std::forward<T>(value));
    m_pvoid = std::static_pointer_cast<void>(ptr);
    m_ti = std::type_index{typeid(StorageT)};
}

template <typename T>
T& VarData::as()
{
    using StorageT = detail::storage_type_t<T>;
    static_assert(!std::is_void_v<StorageT>, "VarData: T cannot be void");

    if (!m_pvoid)
    {
        throw VarDataEmptyError{};
    }
    if (m_ti != std::type_index{typeid(StorageT)})
    {
        throw VarDataTypeError{
            "VarData type mismatch: expected " + std::string{typeid(StorageT).name()} +
            ", got " + std::string{m_ti.name()}
        };
    }
    return *static_cast<StorageT*>(m_pvoid.get());
}

template <typename T>
const T& VarData::as() const
{
    using StorageT = detail::storage_type_t<T>;
    static_assert(!std::is_void_v<StorageT>, "VarData: T cannot be void");

    if (!m_pvoid)
    {
        throw VarDataEmptyError{};
    }
    if (m_ti != std::type_index{typeid(StorageT)})
    {
        throw VarDataTypeError{
            "VarData type mismatch: expected " + std::string{typeid(StorageT).name()} +
            ", got " + std::string{m_ti.name()}
        };
    }
    return *static_cast<const StorageT*>(m_pvoid.get());
}

template <typename T>
T* VarData::try_as() noexcept
{
    using StorageT = detail::storage_type_t<T>;
    static_assert(!std::is_void_v<StorageT>, "VarData: T cannot be void");

    if (!m_pvoid || m_ti != std::type_index{typeid(StorageT)})
    {
        return nullptr;
    }
    return static_cast<StorageT*>(m_pvoid.get());
}

template <typename T>
const T* VarData::try_as() const noexcept
{
    using StorageT = detail::storage_type_t<T>;
    static_assert(!std::is_void_v<StorageT>, "VarData: T cannot be void");

    if (!m_pvoid || m_ti != std::type_index{typeid(StorageT)})
    {
        return nullptr;
    }
    return static_cast<const StorageT*>(m_pvoid.get());
}

template <typename T>
std::shared_ptr<T> VarData::get() const noexcept
{
    using StorageT = detail::storage_type_t<T>;
    static_assert(!std::is_void_v<StorageT>, "VarData: T cannot be void");

    if (!m_pvoid || m_ti != std::type_index{typeid(StorageT)})
    {
        return nullptr;
    }
    return std::static_pointer_cast<StorageT>(m_pvoid);
}

template <typename T>
std::shared_ptr<T> VarData::release()
{
    using StorageT = detail::storage_type_t<T>;
    static_assert(!std::is_void_v<StorageT>, "VarData: T cannot be void");

    if (!m_pvoid || m_ti != std::type_index{typeid(StorageT)})
    {
        return nullptr;
    }
    auto result = std::static_pointer_cast<StorageT>(m_pvoid);
    reset();
    return result;
}

} // namespace crddagt
