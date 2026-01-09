/**
 * @file graph_core.hpp
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/graph_core_enums.hpp"
#include "crddagt/common/graph_core_exceptions.hpp"
#include "crddagt/common/graph_core_diagnostics.hpp"
#include "crddagt/common/exported_graph.hpp"

namespace crddagt
{

/**
 * @brief A mutable builder for constructing task graphs with data flow dependencies.
 *
 * @details
 * `GraphCore` is the central class for constructing a CRDDAGT (Create, Read, Destroy
 * DAG of Tasks) graph. It tracks steps (units of execution), fields (data access points),
 * and the relationships between them.
 *
 * @par Construction workflow
 * 1. Create a `GraphCore` instance.
 * 2. Add steps via `add_step()` - each step represents a unit of work.
 * 3. Add fields via `add_field()` - each field represents a step's access to data
 *    with a specific `Usage` (Create, Read, or Destroy).
 * 4. Link steps via `link_steps()` - explicit execution order constraints.
 * 5. Link fields via `link_fields()` - declare that two fields refer to the same data,
 *    which induces implicit execution order based on their `Usage` values.
 * 6. Call `export_graph()` to produce the final computed graph structure.
 *
 * @par Index requirements
 * Steps and fields are identified by indices. The caller is expected to manage the
 * actual step and field objects externally (e.g., in a `UniqueSharedWeakList`) and
 * pass the corresponding indices to `GraphCore` methods. Indices must be added
 * sequentially starting from 0.
 *
 * @par Validation
 * Graph invariants can be checked eagerly (on each mutation) or lazily (deferred
 * until `get_diagnostics()` or `export_graph()` is called), controlled by the
 * constructor parameter.
 *
 * @par Thread safety
 * - No internal synchronization.
 * - Concurrent access requires external synchronization.
 * - Concurrent reads (const methods) are safe if no concurrent writes occur.
 */
class GraphCore
{
public:
    /**
     * @brief Constructor for GraphCore.
     * @param eager_validation If true, graph invariants are checked eagerly when
     *        adding steps, fields, and links. If false, validation is deferred until
     *        diagnostics are requested or the graph is exported.
     */
    explicit GraphCore(bool eager_validation = true);

    /**
     * @brief Get the current number of steps in the graph.
     * @return The count of steps added via `add_step()`.
     */
    size_t step_count() const noexcept;

    /**
     * @brief Get the current number of fields in the graph.
     * @return The count of fields added via `add_field()`.
     */
    size_t field_count() const noexcept;

    /**
     * @brief Add a step to the graph.
     * @param step_idx Index of the step to add. Must equal the current `step_count()`.
     * @throw GraphCoreError with `InvalidStepIndex` if step_idx != step_count(), or
     *        `DuplicateStepIndex` if the step already exists.
     */
    void add_step(size_t step_idx);

    /**
     * @brief Add a field to the graph.
     * @param step_idx Index of the owning step. Must refer to an existing step.
     * @param field_idx Index of the field to add. Must equal the current `field_count()`.
     * @param ti The type information of the data accessed by this field.
     * @param usage The usage type (Create, Read, or Destroy).
     * @throw GraphCoreError with `InvalidStepIndex` if step_idx is invalid,
     *        `InvalidFieldIndex` if field_idx != field_count(), or
     *        `DuplicateFieldIndex` if the field already exists.
     */
    void add_field(size_t step_idx, size_t field_idx, std::type_index ti, Usage usage);

    /**
     * @brief Link two steps to establish an explicit execution order.
     * @param step_before_idx Index of the step that must execute first.
     * @param step_after_idx Index of the step that must execute after.
     * @param trust The trust level to assign to this link (for diagnostics).
     * @throw GraphCoreError with `InvalidStepIndex` if either index is invalid,
     *        `CycleDetected` if the link would create a cycle (when eager validation
     *        is enabled), or `InvariantViolation` for other constraint violations.
     * @note The order of arguments matters: this creates a directed edge from
     *       `step_before_idx` to `step_after_idx`.
     */
    void link_steps(size_t step_before_idx, size_t step_after_idx, TrustLevel trust);

    /**
     * @brief Link two fields to declare they reference the same data.
     * @param field_one_idx Index of the first field.
     * @param field_two_idx Index of the second field.
     * @param trust The trust level to assign to this link (for diagnostics).
     * @throw GraphCoreError with `InvalidFieldIndex` if either index is invalid,
     *        `TypeMismatch` if the fields have different type information,
     *        `UsageConstraintViolation` if the link would violate usage rules
     *        (e.g., two Creates for the same data), or `CycleDetected` if the
     *        implied step ordering would create a cycle.
     * @note Linking fields induces implicit step execution order based on their
     *       `Usage` values: Create < Read < Destroy.
     */
    void link_fields(size_t field_one_idx, size_t field_two_idx, TrustLevel trust);

    /**
     * @brief Get diagnostics information about the graph.
     * @return Shared pointer to the diagnostics object containing any errors
     *         or warnings detected in the graph structure.
     */
    std::shared_ptr<GraphCoreDiagnostics> get_diagnostics() const;

    /**
     * @brief Export the graph structure.
     * @return Shared pointer to the exported graph containing computed relationships.
     * @throw GraphCoreError with `InvalidState` if the graph has unresolved errors
     *        that prevent export.
     */
    std::shared_ptr<ExportedGraph> export_graph() const;

private:
    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    bool m_eager_validation;

    // -------------------------------------------------------------------------
    // Step tracking
    // -------------------------------------------------------------------------

    /// Number of steps added via add_step().
    size_t m_step_count = 0;

    /// For each step, the list of field indices owned by that step.
    /// Indexed by step index.
    std::vector<std::vector<FieldIdx>> m_step_fields;

    // -------------------------------------------------------------------------
    // Field tracking
    // -------------------------------------------------------------------------

    /// Number of fields added via add_field().
    size_t m_field_count = 0;

    /// The owning step for each field. Indexed by field index.
    std::vector<StepIdx> m_field_owner_step;

    /// The type information for each field. Indexed by field index.
    std::vector<std::type_index> m_field_types;

    /// The usage for each field. Indexed by field index.
    std::vector<Usage> m_field_usages;

    // -------------------------------------------------------------------------
    // Step links (explicit)
    // -------------------------------------------------------------------------

    /// Explicit step-to-step links: (before_step_idx, after_step_idx).
    std::vector<StepLinkPair> m_explicit_step_links;

    /// Trust levels for explicit step links, parallel to m_explicit_step_links.
    std::vector<TrustLevel> m_explicit_step_link_trust;

    // -------------------------------------------------------------------------
    // Field links (equivalence classes)
    // -------------------------------------------------------------------------

    /// Union-find parent array for field equivalence classes.
    /// m_field_uf_parent[i] == i means i is a root.
    mutable std::vector<FieldIdx> m_field_uf_parent;

    /// Union-find rank array for balanced merging.
    mutable std::vector<size_t> m_field_uf_rank;

    /// Field link edges: (field_one_idx, field_two_idx).
    std::vector<std::pair<FieldIdx, FieldIdx>> m_field_links;

    /// Trust levels for field links, parallel to m_field_links.
    std::vector<TrustLevel> m_field_link_trust;

    // -------------------------------------------------------------------------
    // Union-find helpers
    // -------------------------------------------------------------------------

    /// Find the root of the equivalence class containing field_idx.
    FieldIdx uf_find(FieldIdx field_idx) const;

    /// Unite the equivalence classes containing field_a and field_b.
    void uf_unite(FieldIdx field_a, FieldIdx field_b);

    // -------------------------------------------------------------------------
    // Diagnostic helpers
    // -------------------------------------------------------------------------

    /// Add blamed field links to diagnostic item, ordered by trust level.
    void add_field_link_blame(DiagnosticItem& item,
                              const std::vector<FieldIdx>& involved_fields) const;

    /// Add blamed step links to diagnostic item, ordered by trust level.
    void add_step_link_blame(DiagnosticItem& item,
                             const std::vector<StepIdx>& involved_steps) const;
};

} // namespace crddagt
