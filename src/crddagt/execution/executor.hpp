/**
 * @file executor.hpp
 * @brief IExecutor interface and ExecutorConfig.
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/execution/executable_graph.hpp"
#include "crddagt/execution/execution_result.hpp"

namespace crddagt
{

// Forward declaration
class TaskWrapper;
using TaskWrapperPtr = std::shared_ptr<TaskWrapper>;

/**
 * @brief Configuration for executor behavior.
 */
struct ExecutorConfig
{
    /**
     * @brief Number of worker threads.
     * @details 0 means use std::thread::hardware_concurrency().
     *          1 means single-threaded execution.
     */
    size_t thread_count{1};

    /**
     * @brief Whether to collect per-step timing.
     */
    bool collect_timing{false};

    /**
     * @brief Whether to abort on first failure.
     * @details If true, remaining steps are cancelled on first failure.
     *          If false, independent paths continue execution.
     */
    bool abort_on_failure{true};
};

/**
 * @brief Interface for task graph executors.
 *
 * @details
 * IExecutor defines the contract for executing an ExecutableGraph.
 * Implementations may be single-threaded or multi-threaded.
 *
 * @par Thread Safety
 * - execute() may be called from any thread.
 * - request_stop() may be called from any thread during execution.
 * - stop_requested() may be called from any thread.
 */
class IExecutor
{
public:
    virtual ~IExecutor() = default;

    /**
     * @brief Execute a task graph.
     * @param graph The executable graph to run.
     * @return ExecutionResult with outcome details.
     */
    virtual ExecutionResult execute(std::shared_ptr<ExecutableGraph> graph) = 0;

    /**
     * @brief Request graceful stop of execution.
     *
     * @details
     * Sets a flag that workers check. In-progress steps complete normally;
     * pending steps are cancelled. This is cooperative, not preemptive.
     */
    virtual void request_stop() = 0;

    /**
     * @brief Check if stop has been requested.
     * @return True if request_stop() has been called.
     */
    virtual bool stop_requested() const noexcept = 0;
};

/**
 * @brief Base class for Executor implementations.
 *
 * @details
 * Provides common functionality for executors including:
 * - Stop request handling
 * - TaskWrapper creation and management
 * - Successor linkage
 *
 * Derived classes implement the actual scheduling and worker management.
 */
class Executor : public IExecutor, public std::enable_shared_from_this<Executor>
{
public:
    explicit Executor(ExecutorConfig config);
    virtual ~Executor() = default;

    void request_stop() override;
    bool stop_requested() const noexcept override;

    /**
     * @brief Enqueue a task for execution.
     * @param task The task to enqueue.
     * @note Called by TaskWrapper when a successor becomes ready.
     */
    virtual void enqueue(TaskWrapperPtr task) = 0;

    /**
     * @brief Notify that a task has completed.
     * @param task The completed task.
     * @note Called by TaskWrapper after run() completes.
     */
    virtual void notify_completion(TaskWrapper* task) = 0;

protected:
    /**
     * @brief Create TaskWrappers for all steps in the graph.
     * @param graph The executable graph.
     * @return Vector of TaskWrappers indexed by StepIdx.
     */
    std::vector<TaskWrapperPtr> create_task_wrappers(
        std::shared_ptr<ExecutableGraph> graph);

    /**
     * @brief Wire up successor relationships between TaskWrappers.
     * @param wrappers The TaskWrappers to connect.
     * @param graph The executable graph (for successor info).
     */
    void wire_successors(
        std::vector<TaskWrapperPtr>& wrappers,
        const ExecutableGraph& graph);

    ExecutorConfig m_config;
    std::atomic<bool> m_stop_requested{false};
};

} // namespace crddagt
