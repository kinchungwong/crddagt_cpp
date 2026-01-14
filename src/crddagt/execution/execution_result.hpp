/**
 * @file execution_result.hpp
 * @brief Definition of ExecutionResult returned by Executor::execute().
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/graph_core_enums.hpp"

namespace crddagt
{

/**
 * @brief Result of executing a task graph.
 *
 * @details
 * ExecutionResult captures the outcome of running an ExecutableGraph:
 * - Success/failure status
 * - Which steps failed and their errors
 * - Which steps were cancelled
 * - Timing information (if collected)
 */
struct ExecutionResult
{
    /**
     * @brief Overall success status.
     * @details True if all steps completed successfully.
     */
    bool success{true};

    /**
     * @brief Indices of steps that failed.
     */
    std::vector<StepIdx> failed_steps;

    /**
     * @brief Error messages for failed steps, parallel to failed_steps.
     */
    std::vector<std::string> error_messages;

    /**
     * @brief Indices of steps that were cancelled.
     */
    std::vector<StepIdx> cancelled_steps;

    /**
     * @brief Indices of steps that completed successfully.
     */
    std::vector<StepIdx> completed_steps;

    /**
     * @brief Total execution duration (wall-clock time).
     */
    std::chrono::nanoseconds total_duration{0};

    /**
     * @brief Per-step durations, indexed by StepIdx.
     * @details Only populated if timing collection is enabled.
     */
    std::vector<std::chrono::nanoseconds> step_durations;

    /**
     * @brief Check if execution was stopped by request.
     */
    bool stopped{false};

    /**
     * @brief Get a summary string for logging.
     */
    std::string summary() const
    {
        std::string result;
        if (success)
        {
            result = "Execution succeeded";
        }
        else if (stopped)
        {
            result = "Execution stopped by request";
        }
        else
        {
            result = "Execution failed";
        }
        result += " (completed=" + std::to_string(completed_steps.size());
        result += ", failed=" + std::to_string(failed_steps.size());
        result += ", cancelled=" + std::to_string(cancelled_steps.size()) + ")";
        return result;
    }
};

} // namespace crddagt
