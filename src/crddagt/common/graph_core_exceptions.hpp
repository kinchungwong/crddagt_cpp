/**
 * @file graph_core_exceptions.hpp
 */
#pragma once
#include "crddagt/common/common.hpp"

namespace crddagt
{

/**
 * @brief Error codes for GraphCore operations.
 *
 * @note This enumeration is subject to further discovery and change as the
 * GraphCore API evolves. New error codes may be added, and existing codes
 * may be refined or reorganized.
 */
enum class GraphCoreErrorCode
{
    InvalidStepIndex,
    InvalidFieldIndex,
    DuplicateStepIndex,
    DuplicateFieldIndex,
    TypeMismatch,
    UsageConstraintViolation,
    CycleDetected,
    InvalidState,
    InvariantViolation
};

/**
 * @brief Exception class for GraphCore errors.
 *
 * @details
 * `GraphCoreError` is thrown by `GraphCore` methods when preconditions are
 * violated, indices are invalid, or graph invariants would be broken by an
 * operation. Each exception carries an error code and a descriptive message.
 *
 * @par Thread safety
 * - The exception object itself follows standard exception semantics.
 * - Safe to copy and rethrow across threads.
 */
class GraphCoreError : public std::exception
{
public:
    /**
     * @brief Construct a GraphCoreError.
     * @param code The error code indicating the type of error.
     * @param message A descriptive message explaining the error.
     */
    GraphCoreError(GraphCoreErrorCode code, std::string message)
        : m_code(code)
        , m_message(std::move(message))
    {
    }

    /**
     * @brief Get the error code.
     * @return The error code for this exception.
     */
    GraphCoreErrorCode code() const noexcept
    {
        return m_code;
    }

    /**
     * @brief Get the error message.
     * @return A C-string describing the error.
     */
    const char* what() const noexcept override
    {
        return m_message.c_str();
    }

private:
    GraphCoreErrorCode m_code;
    std::string m_message;
};

} // namespace crddagt
