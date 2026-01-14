#include "crddagt/common/graph_builder.hpp"
#include "crddagt/common/vardata.inline.hpp"
#include <sstream>
#include <unordered_set>

namespace crddagt
{

GraphBuilder::GraphBuilder(bool eager_validation)
    : m_eager_validation{eager_validation}
    , m_core{std::make_unique<GraphCore>(eager_validation)}
    , m_steps{}
    , m_fields{}
{}

void GraphBuilder::add_step(const StepPtr& step)
{
    size_t sidx = m_steps.insert(step);
    if (sidx >= m_core->step_count())
    {
        m_core->add_step(sidx);
    }
    auto fields = step->get_fields();
    for (const auto& field : fields)
    {
        add_field(field);
    }
}

void GraphBuilder::add_field(const FieldPtr& field)
{
    StepPtr step = field->get_step();
    size_t sidx = m_steps.insert(step);
    size_t fidx = m_fields.insert(field);
    bool has_added_step = sidx >= m_core->step_count();
    bool has_added_field = fidx >= m_core->field_count();

    // Fix for D1: If step wasn't in GraphCore yet, add it now
    if (has_added_step)
    {
        m_core->add_step(sidx);
    }

    if (has_added_field)
    {
        std::type_index ti = field->get_type();
        Usage usage = field->get_usage();
        m_core->add_field(sidx, fidx, ti, usage);
    }
}

void GraphBuilder::link_steps(const StepPtr& before, const StepPtr& after, TrustLevel trust)
{
    size_t before_idx = m_steps.insert(before);
    size_t after_idx = m_steps.insert(after);
    m_core->link_steps(before_idx, after_idx, trust);
}

void GraphBuilder::link_fields(const FieldPtr& field_one, const FieldPtr& field_two, TrustLevel trust)
{
    size_t field_one_idx = m_fields.insert(field_one);
    size_t field_two_idx = m_fields.insert(field_two);
    m_core->link_fields(field_one_idx, field_two_idx, trust);
}

std::shared_ptr<GraphCoreDiagnostics> GraphBuilder::get_diagnostics(bool treat_as_sealed) const
{
    return m_core->get_diagnostics(treat_as_sealed);
}

std::shared_ptr<ExecutableGraph> GraphBuilder::build()
{
    // Step 1: Get diagnostics with sealed=true (MissingCreate is an error)
    auto diagnostics = m_core->get_diagnostics(/*treat_as_sealed=*/true);
    if (diagnostics->has_errors())
    {
        // Build error message from diagnostics
        std::ostringstream oss;
        oss << "Graph validation failed with " << diagnostics->errors().size() << " error(s):\n";
        for (const auto& err : diagnostics->errors())
        {
            oss << "  - " << err.message << "\n";
        }
        throw GraphValidationError(oss.str(), diagnostics);
    }

    // Step 2: Export the graph structure
    auto exported = m_core->export_graph();

    // Step 3: Create the ExecutableGraph
    auto exec_graph = std::make_shared<ExecutableGraph>();

    const size_t num_steps = m_core->step_count();
    const size_t num_data = exported->data_infos.size();

    // Populate steps vector
    exec_graph->steps.reserve(num_steps);
    for (size_t sidx = 0; sidx < num_steps; ++sidx)
    {
        auto step = m_steps.at(sidx);
        exec_graph->steps.push_back(step);
    }

    // Populate data_objects (one per DataIdx)
    // We need to get the IData from the IField for each equivalence class
    exec_graph->data_objects.reserve(num_data);
    for (const auto& data_info : exported->data_infos)
    {
        // Get the first field in this equivalence class to get the IData
        if (!data_info.field_usages.empty())
        {
            FieldIdx fidx = std::get<1>(data_info.field_usages[0]);
            auto field = m_fields.at(fidx);
            exec_graph->data_objects.push_back(field->get_data());
        }
        else
        {
            // Shouldn't happen with valid graph, but handle gracefully
            exec_graph->data_objects.push_back(nullptr);
        }
    }

    // Compute predecessor counts and successor lists from combined_step_links
    exec_graph->predecessor_counts.resize(num_steps, 0);
    exec_graph->successors.resize(num_steps);

    // Use a set to avoid counting duplicate edges multiple times
    std::vector<std::unordered_set<StepIdx>> predecessor_sets(num_steps);

    for (const auto& [before, after] : exported->combined_step_links)
    {
        if (predecessor_sets[after].insert(before).second)
        {
            // This is a new predecessor for 'after'
            exec_graph->predecessor_counts[after]++;
            exec_graph->successors[before].push_back(after);
        }
    }

    // Assign tokens: step tokens are 1..num_steps, graph token is 0
    exec_graph->graph_token = 0;
    exec_graph->step_tokens.reserve(num_steps);
    for (size_t sidx = 0; sidx < num_steps; ++sidx)
    {
        exec_graph->step_tokens.push_back(sidx + 1);  // Tokens 1, 2, 3, ...
    }

    // Build step_access_rights from data_infos
    exec_graph->step_access_rights.resize(num_steps);
    for (const auto& data_info : exported->data_infos)
    {
        for (const auto& [sidx, fidx, usage] : data_info.field_usages)
        {
            exec_graph->step_access_rights[sidx].emplace_back(data_info.didx, usage);
        }
    }

    // Copy data_infos for reference
    exec_graph->data_infos = exported->data_infos;

    return exec_graph;
}

} // namespace crddagt
