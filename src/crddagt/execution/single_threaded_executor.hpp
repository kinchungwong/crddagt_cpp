/**
 * @file single_threaded_executor.hpp
 * @brief SingleThreadedExecutor for sequential task execution.
 */
#pragma once
#include "crddagt/execution/executor.hpp"
#include <queue>

namespace crddagt
{

/**
 * @brief Single-threaded executor for debugging and testing.
 *
 * @details
 * Executes tasks sequentially in a single thread. Useful for:
 * - Debugging execution issues without thread complexity
 * - Testing graph correctness
 * - Reference implementation for verifying parallel executors
 *
 * @par Thread Safety
 * - execute() is not thread-safe; call from one thread only.
 * - request_stop() can be called from any thread.
 */
class SingleThreadedExecutor : public Executor
{
public:
    /**
     * @brief Construct a single-threaded executor.
     * @param config Configuration (thread_count ignored, always 1).
     */
    explicit SingleThreadedExecutor(ExecutorConfig config = {});

    /**
     * @brief Execute a task graph sequentially.
     * @param graph The executable graph to run.
     * @return ExecutionResult with outcome details.
     */
    ExecutionResult execute(std::shared_ptr<ExecutableGraph> graph) override;

    /**
     * @brief Enqueue a task for execution.
     * @param task The task to enqueue.
     */
    void enqueue(TaskWrapperPtr task) override;

    /**
     * @brief Notify that a task has completed.
     * @param task The completed task.
     */
    void notify_completion(TaskWrapper* task) override;

private:
    // Ready queue (FIFO for fairness)
    std::queue<TaskWrapperPtr> m_ready_queue;

    // Tracking for result building
    std::vector<TaskWrapperPtr> m_all_tasks;
    size_t m_completed_count{0};
};

/**
 * @brief Factory function to create a SingleThreadedExecutor.
 * @param config Configuration options.
 * @return Shared pointer to the executor.
 */
inline std::shared_ptr<SingleThreadedExecutor> make_single_threaded_executor(
    ExecutorConfig config = {})
{
    return std::make_shared<SingleThreadedExecutor>(std::move(config));
}

} // namespace crddagt
