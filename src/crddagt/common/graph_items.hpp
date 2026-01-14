/**
 * @file graph_items.hpp
 * @brief Interfaces for graph items: IStep, IField, IData.
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/vardata.hpp"
#include "crddagt/common/graph_core_enums.hpp"

namespace crddagt
{

class IStep;
class IField;
class IData;

/**
 * @brief A token representing authorization to access or modify data.
 *
 * @details
 * Graphs assign CrdTokens to Steps during build(). Each step receives a unique
 * token that authorizes it to perform specific operations on specific data objects.
 * The graph also reserves a token for itself (for setup/teardown operations).
 *
 * Tokens are validated by IData implementations to enforce access control.
 */
using CrdToken = size_t;

/**
 * @brief Interface for executable steps in a task graph.
 *
 * @details
 * IStep represents a unit of work in the task graph. Implementations must
 * provide the execute() method containing the actual work, plus identification
 * methods for debugging and logging.
 *
 * @par Thread Safety
 * - Implementations must ensure execute() is safe to call from any thread.
 * - State queries (state(), class_name(), etc.) should be safe for concurrent reads.
 * - Implementations should not hold locks during execute() that could cause deadlock.
 *
 * @par Lifecycle
 * - Created by user code and registered with GraphBuilder.
 * - execute() called by TaskWrapper during graph execution.
 * - Object lifetime managed via shared_ptr.
 */
class IStep
{
public:
    virtual ~IStep() = 0;

    /**
     * @brief Get a shared_ptr to this step.
     * @note Implementations must inherit from std::enable_shared_from_this<IStep>.
     */
    virtual std::shared_ptr<IStep> shared_from_this() = 0;

    /**
     * @brief Get all fields associated with this step.
     * @return Vector of field pointers.
     */
    virtual std::vector<std::shared_ptr<IField>> get_fields() = 0;

    /**
     * @brief Execute this step's work.
     *
     * @details
     * This method contains the user-defined work for this step. It is called
     * by the TaskWrapper after all predecessors have completed.
     *
     * @throws Any exception to indicate failure. The exception will be caught
     *         by TaskWrapper and the step will transition to Failed state.
     *
     * @note This method should not block on locks or I/O that could cause
     *       deadlock with other steps in the graph.
     */
    virtual void execute() = 0;

    /**
     * @brief Get the class name for type identification.
     * @return Reference to static string with the implementation class name.
     */
    virtual const std::string& class_name() const = 0;

    /**
     * @brief Get a user-friendly display name for this step.
     * @return String suitable for user-facing output.
     */
    virtual std::string friendly_name() const = 0;

    /**
     * @brief Get a unique identifier string for this step instance.
     * @return String that uniquely identifies this step (e.g., for logging).
     */
    virtual std::string unique_name() const = 0;

protected:
    IStep() = default;

private:
    IStep(const IStep&) = delete;
    IStep(IStep&&) = delete;
    IStep& operator=(const IStep&) = delete;
    IStep& operator=(IStep&&) = delete;
};

/**
 * @brief Interface for fields that connect steps to data.
 *
 * @details
 * A field represents a step's relationship to a piece of data, including
 * the type of access (Create, Read, or Destroy).
 *
 * @par Thread Safety
 * - All methods should be safe for concurrent reads.
 * - Fields are typically immutable after construction.
 */
class IField
{
public:
    virtual ~IField() = 0;

    /**
     * @brief Get a shared_ptr to this field.
     */
    virtual std::shared_ptr<IField> shared_from_this() = 0;

    /**
     * @brief Get the step that owns this field.
     */
    virtual std::shared_ptr<IStep> get_step() = 0;

    /**
     * @brief Get the data object this field references.
     */
    virtual std::shared_ptr<IData> get_data() = 0;

    /**
     * @brief Get the type of data this field handles.
     */
    virtual std::type_index get_type() = 0;

    /**
     * @brief Get the usage type (Create, Read, or Destroy).
     */
    virtual Usage get_usage() = 0;

protected:
    IField() = default;

private:
    IField(const IField&) = delete;
    IField(IField&&) = delete;
    IField& operator=(const IField&) = delete;
    IField& operator=(IField&&) = delete;
};

/**
 * @brief Interface for data objects that hold values accessed by steps.
 *
 * @details
 * IData represents a piece of data that flows through the graph. It is
 * accessed by steps via their fields, with access controlled by CrdTokens.
 *
 * @par Thread Safety Requirements
 * Implementations MUST support the following concurrent access patterns:
 *
 * - **Create**: Exclusive access. No concurrent access allowed.
 *   Only one step may call set_value() and it must have the Create usage.
 *
 * - **Read**: Shared access. Multiple concurrent get_value() calls are OK.
 *   Read steps may execute in parallel.
 *
 * - **Destroy**: Exclusive access. No concurrent access allowed.
 *   Only one step may call remove_value() and it must be the last accessor.
 *
 * This matches the CRD (Create-Read-Destroy) semantics enforced by the graph.
 * Implementations should use std::shared_mutex or equivalent for thread-safe
 * access when parallel execution is enabled.
 *
 * @par Token Validation
 * Implementations should validate that the provided token authorizes the
 * requested operation. Invalid tokens should result in an exception.
 */
class IData
{
public:
    virtual ~IData() = 0;

    /**
     * @brief Get a shared_ptr to this data object.
     */
    virtual std::shared_ptr<IData> shared_from_this() = 0;

    /**
     * @brief Set the value (Create operation).
     * @param token Authorization token for this operation.
     * @param value The value to store.
     * @throws std::runtime_error if token is invalid or operation not allowed.
     */
    virtual void set_value(CrdToken token, VarData value) = 0;

    /**
     * @brief Get the value (Read operation).
     * @param token Authorization token for this operation.
     * @return Copy of the stored VarData.
     * @throws std::runtime_error if token is invalid, operation not allowed,
     *         or value has not been set.
     */
    virtual VarData get_value(CrdToken token) = 0;

    /**
     * @brief Remove and return the value (Destroy operation).
     * @param token Authorization token for this operation.
     * @return The stored VarData (ownership transferred to caller).
     * @throws std::runtime_error if token is invalid, operation not allowed,
     *         or value has not been set.
     */
    virtual VarData remove_value(CrdToken token) = 0;

protected:
    IData() = default;

private:
    IData(const IData&) = delete;
    IData(IData&&) = delete;
    IData& operator=(const IData&) = delete;
    IData& operator=(IData&&) = delete;
};

using StepPtr = std::shared_ptr<IStep>;
using FieldPtr = std::shared_ptr<IField>;
using DataPtr = std::shared_ptr<IData>;

inline IStep::~IStep() = default;
inline IField::~IField() = default;
inline IData::~IData() = default;

} // namespace crddagt
