#include "crddagt/execution/executor.hpp"
#include "crddagt/execution/task_wrapper.hpp"

namespace crddagt
{

Executor::Executor(ExecutorConfig config)
    : m_config{std::move(config)}
{}

void Executor::request_stop()
{
    m_stop_requested.store(true, std::memory_order_release);
}

bool Executor::stop_requested() const noexcept
{
    return m_stop_requested.load(std::memory_order_acquire);
}

std::vector<TaskWrapperPtr> Executor::create_task_wrappers(
    std::shared_ptr<ExecutableGraph> graph)
{
    std::vector<TaskWrapperPtr> wrappers;
    wrappers.reserve(graph->step_count());

    auto self = shared_from_this();

    for (size_t sidx = 0; sidx < graph->step_count(); ++sidx)
    {
        auto wrapper = std::make_shared<TaskWrapper>(
            graph->steps[sidx],
            sidx,
            graph->step_tokens[sidx],
            graph->predecessor_counts[sidx],
            self
        );
        wrappers.push_back(wrapper);
    }

    return wrappers;
}

void Executor::wire_successors(
    std::vector<TaskWrapperPtr>& wrappers,
    const ExecutableGraph& graph)
{
    for (size_t sidx = 0; sidx < graph.step_count(); ++sidx)
    {
        for (StepIdx succ_idx : graph.successors[sidx])
        {
            wrappers[sidx]->add_successor(wrappers[succ_idx]);
        }
    }
}

} // namespace crddagt
