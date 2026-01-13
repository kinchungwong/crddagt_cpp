/**
 * @file vardata.hpp
 * @brief Definition of VarData class for type-erased variable storage.
 * @see vardata.inline.hpp for implementations of type-parameterized methods.
 */

#pragma once
#include "crddagt/common/common.hpp"

namespace crddagt
{

/**
 * @brief Exception thrown when VarData type access fails.
 */
class VarDataTypeError : public std::runtime_error
{
public:
    explicit VarDataTypeError(const std::string& msg)
        : std::runtime_error(msg)
    {}
};

/**
 * @brief Exception thrown when accessing empty VarData.
 */
class VarDataEmptyError : public std::runtime_error
{
public:
    VarDataEmptyError()
        : std::runtime_error("VarData is empty")
    {}
};

/**
 * @brief A type-erased container for storing a variable of any type.
 *
 * @details
 * VarData provides type-safe storage for any movable/copyable type using
 * `shared_ptr<void>` with a custom deleter that preserves the original type.
 *
 * @par Invariants
 * - `(m_ti == typeid(void))` if and only if `m_pvoid == nullptr`
 * - Template parameter `T` cannot be void, cv-qualified, array, or reference
 *
 * @par Thread Safety
 * - Safe for simultaneous reading and being copied from.
 * - No internal mutex; synchronization is caller's responsibility for writes.
 * - Owned by Data class (IData impl) which manages access control.
 *
 * @par Ownership
 * - Value-like semantics with shared ownership of the underlying data.
 * - Multiple VarData instances can share the same underlying storage.
 */
class VarData
{
public:
    /**
     * @brief Default constructor creates empty VarData.
     */
    VarData() = default;

    /**
     * @brief Check if VarData contains a value.
     * @return true if contains a value, false if empty.
     */
    [[nodiscard]] bool has_value() const noexcept
    {
        return m_pvoid != nullptr;
    }

    /**
     * @brief Check if stored type matches T.
     * @tparam T The type to check against (must be decayed).
     * @return true if VarData contains a value of type T.
     */
    template <typename T>
    [[nodiscard]] bool has_type() const noexcept;

    /**
     * @brief Get the type_index of stored value.
     * @return type_index of stored type, or typeid(void) if empty.
     */
    [[nodiscard]] std::type_index type() const noexcept
    {
        return m_ti;
    }

    /**
     * @brief Clear the stored value.
     * @post has_value() == false
     */
    void reset() noexcept
    {
        m_pvoid.reset();
        m_ti = std::type_index{typeid(void)};
    }

    /**
     * @brief Construct a value of type T in-place.
     * @tparam T The type to construct (will be decayed).
     * @tparam Args Constructor argument types.
     * @param args Constructor arguments forwarded to T's constructor.
     * @post has_value() == true && has_type<T>() == true
     */
    template <typename T, typename... Args>
    void emplace(Args&&... args);

    /**
     * @brief Store a value by copy or move.
     * @tparam T The type of value (will be decayed).
     * @param value The value to store.
     * @post has_value() == true
     */
    template <typename T>
    void set(T&& value);

    /**
     * @brief Access stored value as reference.
     * @tparam T The expected type.
     * @return Reference to stored value.
     * @throws VarDataEmptyError if empty.
     * @throws VarDataTypeError if type mismatch.
     */
    template <typename T>
    [[nodiscard]] T& as();

    /**
     * @brief Access stored value as const reference.
     * @tparam T The expected type.
     * @return Const reference to stored value.
     * @throws VarDataEmptyError if empty.
     * @throws VarDataTypeError if type mismatch.
     */
    template <typename T>
    [[nodiscard]] const T& as() const;

    /**
     * @brief Try to access stored value as pointer.
     * @tparam T The expected type.
     * @return Pointer to stored value, or nullptr if empty or type mismatch.
     */
    template <typename T>
    [[nodiscard]] T* try_as() noexcept;

    /**
     * @brief Try to access stored value as const pointer.
     * @tparam T The expected type.
     * @return Const pointer to stored value, or nullptr if empty or type mismatch.
     */
    template <typename T>
    [[nodiscard]] const T* try_as() const noexcept;

    /**
     * @brief Get shared_ptr to stored value.
     * @tparam T The expected type.
     * @return shared_ptr<T> if type matches, nullptr otherwise.
     */
    template <typename T>
    [[nodiscard]] std::shared_ptr<T> get() const noexcept;

    /**
     * @brief Release ownership and return as shared_ptr.
     * @tparam T The expected type.
     * @return shared_ptr<T> if type matches, nullptr otherwise.
     * @post If successful, this VarData is reset to empty.
     */
    template <typename T>
    [[nodiscard]] std::shared_ptr<T> release();

private:
    std::shared_ptr<void> m_pvoid{};
    std::type_index m_ti{typeid(void)};
};

} // namespace crddagt
