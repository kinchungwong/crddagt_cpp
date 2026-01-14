#include "crddagt/execution/single_threaded_executor.hpp"
#include "crddagt/execution/task_wrapper.hpp"

namespace crddagt
{

SingleThreadedExecutor::SingleThreadedExecutor(ExecutorConfig config)
    : Executor(std::move(config))
{}

ExecutionResult SingleThreadedExecutor::execute(std::shared_ptr<ExecutableGraph> graph)
{
    ExecutionResult result;
    auto start_time = std::chrono::steady_clock::now();

    // Reset internal state (but not stop flag - that's set externally)
    while (!m_ready_queue.empty()) m_ready_queue.pop();
    m_completed_count = 0;

    // Handle empty graph
    if (graph->step_count() == 0)
    {
        result.success = true;
        auto end_time = std::chrono::steady_clock::now();
        result.total_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time);
        return result;
    }

    // Create task wrappers
    m_all_tasks = create_task_wrappers(graph);

    // Wire successor relationships
    wire_successors(m_all_tasks, *graph);

    // Initialize timing storage if requested
    if (m_config.collect_timing)
    {
        result.step_durations.resize(graph->step_count(), std::chrono::nanoseconds{0});
    }

    // Enqueue initially ready tasks
    for (auto& task : m_all_tasks)
    {
        if (task->is_ready())
        {
            task->mark_queued();
            m_ready_queue.push(task);
        }
    }

    // Process queue until empty or stopped
    while (!m_ready_queue.empty() && !m_stop_requested.load())
    {
        auto task = m_ready_queue.front();
        m_ready_queue.pop();

        // Execute the task
        task->run();
    }

    // Build result
    auto end_time = std::chrono::steady_clock::now();
    result.total_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time);

    result.success = true;
    result.stopped = m_stop_requested.load();

    for (const auto& task : m_all_tasks)
    {
        StepState state = task->state();
        StepIdx sidx = task->step_idx();

        switch (state)
        {
            case StepState::Succeeded:
                result.completed_steps.push_back(sidx);
                if (m_config.collect_timing)
                {
                    result.step_durations[sidx] = task->duration();
                }
                break;

            case StepState::Failed:
                result.success = false;
                result.failed_steps.push_back(sidx);
                if (task->exception())
                {
                    try
                    {
                        std::rethrow_exception(task->exception());
                    }
                    catch (const std::exception& e)
                    {
                        result.error_messages.push_back(e.what());
                    }
                    catch (...)
                    {
                        result.error_messages.push_back("Unknown exception");
                    }
                }
                else
                {
                    result.error_messages.push_back("Unknown error");
                }
                break;

            case StepState::Cancelled:
            case StepState::NotReady:
            case StepState::Ready:
            case StepState::Queued:
                result.cancelled_steps.push_back(sidx);
                if (!result.stopped && result.failed_steps.empty())
                {
                    // Unexpected state - something went wrong
                    result.success = false;
                }
                break;

            case StepState::Executing:
                // Should not happen in single-threaded executor
                result.success = false;
                result.failed_steps.push_back(sidx);
                result.error_messages.push_back("Task stuck in Executing state");
                break;
        }
    }

    // If stopped, mark as not successful
    if (result.stopped && !result.cancelled_steps.empty())
    {
        result.success = false;
    }

    // Clear task references
    m_all_tasks.clear();

    return result;
}

void SingleThreadedExecutor::enqueue(TaskWrapperPtr task)
{
    m_ready_queue.push(std::move(task));
}

void SingleThreadedExecutor::notify_completion(TaskWrapper* task)
{
    m_completed_count++;

    // In single-threaded execution, we don't need to do anything special here.
    // The main loop handles everything.

    // If abort_on_failure is set and task failed, trigger stop
    if (m_config.abort_on_failure && task->state() == StepState::Failed)
    {
        request_stop();
    }
}

} // namespace crddagt
