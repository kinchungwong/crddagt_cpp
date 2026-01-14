/**
 * @file task_wrapper.hpp
 * @brief TaskWrapper wraps IStep for execution with framework orchestration.
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/graph_core_enums.hpp"
#include "crddagt/common/graph_items.hpp"

namespace crddagt
{

// Forward declarations
class Executor;
class TaskWrapper;

using TaskWrapperPtr = std::shared_ptr<TaskWrapper>;
using TaskWrapperWeakPtr = std::weak_ptr<TaskWrapper>;

/**
 * @brief Wraps an IStep for execution with pre/post orchestration.
 *
 * @details
 * TaskWrapper is the unit of work sent to workers. It handles:
 * - Pre-execution: state transition, stop check, timing start
 * - User code execution: calling step->execute()
 * - Post-execution: state transition, timing, successor notification
 *
 * TaskWrapper directly decrements predecessor counts on successors and
 * submits newly-ready tasks to the queue, enabling decentralized orchestration.
 *
 * @par Ownership Model
 * - Executor owns all TaskWrapper instances via shared_ptr.
 * - TaskWrappers hold weak_ptr to each other (successors).
 * - TaskWrappers hold weak_ptr to Executor (for queue access).
 * - Workers receive references, don't own TaskWrappers.
 *
 * @par Thread Safety
 * - State and predecessor count use atomics (lock-free).
 * - Successor list is immutable after setup.
 * - Multiple threads may call decrement_predecessor_count() concurrently.
 */
class TaskWrapper : public std::enable_shared_from_this<TaskWrapper>
{
public:
    /**
     * @brief Construct a TaskWrapper.
     * @param step The step to wrap.
     * @param step_idx Index of this step in the graph.
     * @param token Authorization token for data access.
     * @param predecessor_count Number of predecessors that must complete.
     * @param executor Weak reference to the owning executor.
     */
    TaskWrapper(
        StepPtr step,
        StepIdx step_idx,
        CrdToken token,
        size_t predecessor_count,
        std::weak_ptr<Executor> executor
    );

    // Non-copyable, non-movable
    TaskWrapper(const TaskWrapper&) = delete;
    TaskWrapper(TaskWrapper&&) = delete;
    TaskWrapper& operator=(const TaskWrapper&) = delete;
    TaskWrapper& operator=(TaskWrapper&&) = delete;

    /**
     * @brief Add a successor that depends on this task.
     * @param successor Weak pointer to the successor TaskWrapper.
     * @note Must be called during setup, before execution starts.
     */
    void add_successor(TaskWrapperWeakPtr successor);

    /**
     * @brief Execute this task (called by Worker).
     *
     * @details
     * Performs the full execution lifecycle:
     * 1. Check stop flag
     * 2. Transition to Executing state
     * 3. Call step->execute()
     * 4. Catch exceptions, transition to Succeeded/Failed
     * 5. Notify successors, enqueue ready ones
     * 6. Notify Executor of completion
     */
    void run();

    /**
     * @brief Get current execution state.
     * @return The current StepState.
     */
    StepState state() const noexcept
    {
        return m_state.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if task is ready to execute.
     * @return True if predecessor count is 0.
     */
    bool is_ready() const noexcept
    {
        return m_predecessors_remaining.load(std::memory_order_acquire) == 0;
    }

    /**
     * @brief Decrement predecessor count (called by predecessor's run()).
     * @return True if this call caused the task to become ready (count reached 0).
     */
    bool decrement_predecessor_count();

    /**
     * @brief Mark this task as cancelled.
     * @pre Task must be in NotReady, Ready, or Queued state.
     */
    void cancel();

    /**
     * @brief Transition from Ready to Queued state.
     * @return True if transition succeeded.
     */
    bool mark_queued();

    /**
     * @brief Get the captured exception (if Failed).
     * @return exception_ptr, or nullptr if not failed.
     */
    std::exception_ptr exception() const noexcept
    {
        return m_exception;
    }

    /**
     * @brief Get execution duration.
     * @return Duration of execute() call, or zero if not completed.
     */
    std::chrono::nanoseconds duration() const noexcept
    {
        return m_duration;
    }

    /**
     * @brief Get the wrapped step.
     */
    StepPtr step() const noexcept
    {
        return m_step;
    }

    /**
     * @brief Get the step index.
     */
    StepIdx step_idx() const noexcept
    {
        return m_step_idx;
    }

    /**
     * @brief Get the authorization token.
     */
    CrdToken token() const noexcept
    {
        return m_token;
    }

private:
    /**
     * @brief Transition state atomically.
     * @param expected The expected current state.
     * @param desired The desired new state.
     * @return True if transition succeeded.
     */
    bool transition_state(StepState expected, StepState desired);

    /**
     * @brief Notify all successors of completion.
     */
    void notify_successors();

    // Configuration (immutable after construction)
    StepPtr m_step;
    StepIdx m_step_idx;
    CrdToken m_token;
    std::weak_ptr<Executor> m_executor;
    std::vector<TaskWrapperWeakPtr> m_successors;

    // Execution state (atomic)
    std::atomic<StepState> m_state{StepState::NotReady};
    std::atomic<size_t> m_predecessors_remaining;

    // Results (written once after execution)
    std::exception_ptr m_exception{};
    std::chrono::nanoseconds m_duration{0};
};

} // namespace crddagt
