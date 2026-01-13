#include "crddagt/execution/task_wrapper.hpp"
#include "crddagt/execution/executor.hpp"

namespace crddagt
{

TaskWrapper::TaskWrapper(
    StepPtr step,
    StepIdx step_idx,
    CrdToken token,
    size_t predecessor_count,
    std::weak_ptr<Executor> executor)
    : m_step{std::move(step)}
    , m_step_idx{step_idx}
    , m_token{token}
    , m_executor{std::move(executor)}
    , m_predecessors_remaining{predecessor_count}
{
    // If no predecessors, start in Ready state
    if (predecessor_count == 0)
    {
        m_state.store(StepState::Ready, std::memory_order_release);
    }
}

void TaskWrapper::add_successor(TaskWrapperWeakPtr successor)
{
    m_successors.push_back(std::move(successor));
}

void TaskWrapper::run()
{
    // Get executor reference
    auto executor = m_executor.lock();
    if (!executor)
    {
        // Executor gone, abort silently
        return;
    }

    // Check stop request before starting
    if (executor->stop_requested())
    {
        m_state.store(StepState::Cancelled, std::memory_order_release);
        executor->notify_completion(this);
        return;
    }

    // Transition to Executing
    StepState expected = StepState::Queued;
    if (!m_state.compare_exchange_strong(expected, StepState::Executing,
                                          std::memory_order_acq_rel))
    {
        // State was not Queued (maybe already cancelled)
        // Just notify completion
        executor->notify_completion(this);
        return;
    }

    // Record start time
    auto start_time = std::chrono::steady_clock::now();

    // Execute user code
    try
    {
        m_step->execute();
        m_state.store(StepState::Succeeded, std::memory_order_release);
    }
    catch (...)
    {
        m_exception = std::current_exception();
        m_state.store(StepState::Failed, std::memory_order_release);
    }

    // Record end time and duration
    auto end_time = std::chrono::steady_clock::now();
    m_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);

    // Notify successors
    notify_successors();

    // Notify executor
    executor->notify_completion(this);
}

bool TaskWrapper::decrement_predecessor_count()
{
    size_t prev = m_predecessors_remaining.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1)
    {
        // We were the last predecessor - task is now ready
        m_state.store(StepState::Ready, std::memory_order_release);
        return true;
    }
    return false;
}

void TaskWrapper::cancel()
{
    StepState current = m_state.load(std::memory_order_acquire);

    // Only cancel if not already in a terminal state or executing
    if (current == StepState::NotReady ||
        current == StepState::Ready ||
        current == StepState::Queued)
    {
        m_state.store(StepState::Cancelled, std::memory_order_release);
    }
}

bool TaskWrapper::mark_queued()
{
    StepState expected = StepState::Ready;
    return m_state.compare_exchange_strong(expected, StepState::Queued,
                                            std::memory_order_acq_rel);
}

bool TaskWrapper::transition_state(StepState expected, StepState desired)
{
    return m_state.compare_exchange_strong(expected, desired,
                                            std::memory_order_acq_rel);
}

void TaskWrapper::notify_successors()
{
    auto executor = m_executor.lock();
    if (!executor)
    {
        return;
    }

    for (auto& weak_succ : m_successors)
    {
        if (auto succ = weak_succ.lock())
        {
            if (succ->decrement_predecessor_count())
            {
                // Successor is now ready - transition to Queued and enqueue
                // (decrement_predecessor_count already set state to Ready)
                succ->mark_queued();
                executor->enqueue(succ);
            }
        }
    }
}

} // namespace crddagt
