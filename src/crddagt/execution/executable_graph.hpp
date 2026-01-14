/**
 * @file executable_graph.hpp
 * @brief Definition of ExecutableGraph structure for task execution.
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/graph_core_enums.hpp"
#include "crddagt/common/graph_items.hpp"
#include "crddagt/common/exported_graph.hpp"

namespace crddagt
{

/**
 * @brief Immutable execution plan produced by GraphBuilder::build().
 *
 * @details
 * ExecutableGraph contains all information needed to execute a validated task graph:
 * - Step objects in execution order
 * - Data objects (one per field equivalence class)
 * - Predecessor counts for ready-queue tracking
 * - Successor lists for completion notification
 * - Token assignments for access control
 *
 * @par Thread Safety
 * - Once constructed, the structure is immutable.
 * - Concurrent reads are safe.
 * - Execution state is tracked externally (in TaskWrapper).
 *
 * @par Ownership
 * - ExecutableGraph owns shared_ptrs to steps and data objects.
 * - GraphBuilder releases ownership after build().
 */
struct ExecutableGraph
{
    /**
     * @brief Step objects indexed by StepIdx.
     *
     * @details
     * The actual step implementations. Order matches GraphCore's step indices.
     */
    std::vector<StepPtr> steps;

    /**
     * @brief Data objects indexed by DataIdx.
     *
     * @details
     * One data object per field equivalence class. Multiple fields may
     * share the same data object.
     */
    std::vector<DataPtr> data_objects;

    /**
     * @brief Number of predecessors for each step.
     *
     * @details
     * predecessor_counts[sidx] is the number of steps that must complete
     * before step sidx can be queued for execution. Steps with count 0
     * are immediately ready.
     */
    std::vector<size_t> predecessor_counts;

    /**
     * @brief Successor lists for each step.
     *
     * @details
     * successors[sidx] contains the indices of steps that depend on sidx.
     * When sidx completes, each successor's predecessor count is decremented.
     */
    std::vector<std::vector<StepIdx>> successors;

    /**
     * @brief Authorization tokens assigned to each step.
     *
     * @details
     * step_tokens[sidx] is the token authorizing step sidx to access its data.
     * Tokens are unique per step.
     */
    std::vector<CrdToken> step_tokens;

    /**
     * @brief Token reserved for graph-level operations.
     *
     * @details
     * Used by the executor for setup/teardown operations that aren't
     * associated with a specific step.
     */
    CrdToken graph_token{0};

    /**
     * @brief Access rights for each step.
     *
     * @details
     * step_access_rights[sidx] contains (DataIdx, Usage) pairs describing
     * what data this step can access and with what operation.
     */
    std::vector<std::vector<std::pair<DataIdx, Usage>>> step_access_rights;

    /**
     * @brief Information about each data object.
     *
     * @details
     * Copied from ExportedGraph::data_infos for reference during execution.
     */
    std::vector<DataInfo> data_infos;

    /**
     * @brief Get indices of steps with no predecessors.
     * @return Vector of step indices that are immediately ready.
     */
    std::vector<StepIdx> get_initial_ready_steps() const
    {
        std::vector<StepIdx> result;
        for (size_t i = 0; i < predecessor_counts.size(); ++i)
        {
            if (predecessor_counts[i] == 0)
            {
                result.push_back(i);
            }
        }
        return result;
    }

    /**
     * @brief Get the total number of steps.
     */
    size_t step_count() const noexcept
    {
        return steps.size();
    }

    /**
     * @brief Get the total number of data objects.
     */
    size_t data_count() const noexcept
    {
        return data_objects.size();
    }
};

} // namespace crddagt
