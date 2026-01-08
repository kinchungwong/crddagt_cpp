/**
 * @file exported_graph.hpp
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/graph_core_enums.hpp"

namespace crddagt
{

// ============================================================================
// Type aliases for interoperability
// ============================================================================

/**
 * @brief Association of a field to its data object, as (field, data).
 * @details Fields that are linked together share the same data object index.
 */
using FieldDataPair = std::pair<FieldIdx, DataIdx>;

/**
 * @brief Information about a data object and all its associated fields.
 */
struct DataInfo
{
    DataIdx didx;
    std::type_index ti;
    std::vector<std::tuple<StepIdx, FieldIdx, Usage>> field_usages;
};

/**
 * @brief Execution order dependency between two steps, as (before, after).
 */
using StepLinkPair = std::pair<StepIdx, StepIdx>;

// ============================================================================
// ExportedGraph
// ============================================================================

/**
 * @brief A snapshot of a computed graph structure.
 *
 * @details
 * `ExportedGraph` represents the result of finalizing a `GraphCore` instance.
 * It contains the computed relationships between fields and data objects,
 * as well as the execution order dependencies between steps.
 *
 * This structure is produced by `GraphCore::export_graph()` and is intended
 * to be consumed by downstream execution engines or analysis tools.
 *
 * @par Data model
 * - **Data object**: An abstract identifier representing a piece of data that
 *   flows through the graph. Fields that are linked together (via `link_fields`)
 *   refer to the same data object.
 * - **Step links**: Directed edges indicating execution order. Step A must
 *   complete before step B if there is a link (A, B).
 *
 * @par Ownership and movement
 * - Fields are non-const to allow upstream code to move (take ownership of)
 *   the vectors rather than copying them.
 * - After moving a field, the `ExportedGraph` instance should be considered
 *   partially consumed.
 *
 * @par Thread safety
 * - No internal synchronization.
 * - Once constructed, the data is conceptually immutable.
 * - Concurrent reads are safe; modification after construction is discouraged.
 *
 * @note The members of this struct are illustrative of the semantic content.
 * The actual representation may evolve as the design matures.
 */
struct ExportedGraph
{
    /**
     * @brief The association of fields to data objects.
     *
     * @details
     * Each `FieldDataPair` is `(field_idx, data_object_idx)`. Fields that are
     * linked together share the same `data_object_idx`. Every field appears
     * exactly once in this vector.
     */
    std::vector<FieldDataPair> field_data_pairs;

    /**
     * @brief Information about each data object and its associated fields.
     */
    std::vector<DataInfo> data_infos;

    /**
     * @brief Execution order induced on steps due to field usages.
     *
     * @details
     * Each `StepLinkPair` is `(before_step_idx, after_step_idx)`, meaning the
     * step at `before_step_idx` must execute before the step at `after_step_idx`.
     * These links are automatically derived from field `Usage` values:
     * Create < Read < Destroy.
     */
    std::vector<StepLinkPair> implicit_step_links;

    /**
     * @brief Execution order induced on steps by explicit step-to-step links.
     *
     * @details
     * Each `StepLinkPair` is `(before_step_idx, after_step_idx)`. These links
     * come directly from `GraphCore::link_steps()` calls.
     */
    std::vector<StepLinkPair> explicit_step_links;

    /**
     * @brief Combined execution order on steps.
     *
     * @details
     * Each `StepLinkPair` is `(before_step_idx, after_step_idx)`. This is the
     * union of `implicit_step_links` and `explicit_step_links`, representing
     * all execution order constraints.
     */
    std::vector<StepLinkPair> combined_step_links;
};

} // namespace crddagt
