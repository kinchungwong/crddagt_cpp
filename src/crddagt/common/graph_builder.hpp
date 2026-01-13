/**
 * @file graph_builder.hpp
 * @brief GraphBuilder bridges user objects to index-based GraphCore.
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/graph_core.hpp"
#include "crddagt/common/graph_items.hpp"
#include "crddagt/common/unique_shared_weak_list.hpp"
#include "crddagt/common/vardata.hpp"
#include "crddagt/execution/executable_graph.hpp"

namespace crddagt
{

/**
 * @brief Exception thrown when graph validation fails at build().
 */
class GraphValidationError : public std::runtime_error
{
public:
    explicit GraphValidationError(const std::string& msg,
                                   std::shared_ptr<GraphCoreDiagnostics> diagnostics)
        : std::runtime_error(msg)
        , m_diagnostics(std::move(diagnostics))
    {}

    /**
     * @brief Get the diagnostics that caused the validation failure.
     */
    const std::shared_ptr<GraphCoreDiagnostics>& diagnostics() const noexcept
    {
        return m_diagnostics;
    }

private:
    std::shared_ptr<GraphCoreDiagnostics> m_diagnostics;
};

/**
 * @brief Builder class that bridges user objects to index-based GraphCore.
 *
 * @details
 * GraphBuilder manages the mapping between user-provided step and field objects
 * and the index-based GraphCore. It provides a convenient API for constructing
 * task graphs and produces an ExecutableGraph for execution.
 *
 * @par Usage
 * 1. Create a GraphBuilder with eager or deferred validation.
 * 2. Add steps via add_step() - each step is registered with GraphCore.
 * 3. Add fields via add_field() - fields are registered with their owning step.
 * 4. Link steps via link_steps() - establish explicit execution order.
 * 5. Link fields via link_fields() - declare fields reference the same data.
 * 6. Call build() to validate and produce an ExecutableGraph.
 *
 * @par Thread Safety
 * - No internal synchronization.
 * - Concurrent access requires external synchronization.
 */
class GraphBuilder
{
public:
    /**
     * @brief Construct a GraphBuilder.
     * @param eager_validation If true, validate on each mutation.
     */
    explicit GraphBuilder(bool eager_validation);

    /**
     * @brief Add a step to the graph.
     * @param step The step to add.
     */
    void add_step(const StepPtr& step);

    /**
     * @brief Add a field to the graph.
     * @param field The field to add.
     * @note If the field's owning step hasn't been added, it will be added automatically.
     */
    void add_field(const FieldPtr& field);

    /**
     * @brief Link two steps to establish explicit execution order.
     * @param before Step that must execute first.
     * @param after Step that must execute after.
     * @param trust Trust level for this link.
     */
    void link_steps(const StepPtr& before, const StepPtr& after, TrustLevel trust);

    /**
     * @brief Link two fields to declare they reference the same data.
     * @param field_one First field.
     * @param field_two Second field.
     * @param trust Trust level for this link.
     */
    void link_fields(const FieldPtr& field_one, const FieldPtr& field_two, TrustLevel trust);

    /**
     * @brief Validate the graph and produce an ExecutableGraph.
     *
     * @return Shared pointer to the executable graph.
     * @throws GraphValidationError if the graph has validation errors.
     *
     * @post After successful build(), this GraphBuilder should not be reused.
     */
    std::shared_ptr<ExecutableGraph> build();

    /**
     * @brief Get diagnostics without building.
     * @param treat_as_sealed If true, MissingCreate is an error.
     * @return Diagnostics from the underlying GraphCore.
     */
    std::shared_ptr<GraphCoreDiagnostics> get_diagnostics(bool treat_as_sealed = false) const;

    /**
     * @brief Get the current step count.
     */
    size_t step_count() const noexcept { return m_core->step_count(); }

    /**
     * @brief Get the current field count.
     */
    size_t field_count() const noexcept { return m_core->field_count(); }

private:
    bool m_eager_validation{};
    std::unique_ptr<GraphCore> m_core{};
    UniqueSharedWeakList<IStep> m_steps{};
    UniqueSharedWeakList<IField> m_fields{};
};

} // namespace crddagt
