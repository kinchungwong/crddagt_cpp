/**
 * @file graph_core.cpp
 */
#include "crddagt/common/graph_core.hpp"

namespace crddagt
{

// ============================================================================
// Constructor
// ============================================================================

GraphCore::GraphCore(bool eager_validation)
    : m_eager_validation(eager_validation)
{
}

// ============================================================================
// Query methods
// ============================================================================

size_t GraphCore::step_count() const noexcept
{
    return m_step_count;
}

size_t GraphCore::field_count() const noexcept
{
    return m_field_count;
}

// ============================================================================
// Step and field management
// ============================================================================

void GraphCore::add_step(size_t step_idx)
{
    if (step_idx != m_step_count)
    {
        if (step_idx < m_step_count)
        {
            throw GraphCoreError(
                GraphCoreErrorCode::DuplicateStepIndex,
                "Step index " + std::to_string(step_idx) + " already exists");
        }
        throw GraphCoreError(
            GraphCoreErrorCode::InvalidStepIndex,
            "Step index " + std::to_string(step_idx) + " is out of sequence; expected " +
                std::to_string(m_step_count));
    }

    m_step_fields.emplace_back();
    ++m_step_count;
}

void GraphCore::add_field(size_t step_idx, size_t field_idx, std::type_index ti, Usage usage)
{
    // Validate step index
    if (step_idx >= m_step_count)
    {
        throw GraphCoreError(
            GraphCoreErrorCode::InvalidStepIndex,
            "Step index " + std::to_string(step_idx) + " does not exist");
    }

    // Validate field index
    if (field_idx != m_field_count)
    {
        if (field_idx < m_field_count)
        {
            throw GraphCoreError(
                GraphCoreErrorCode::DuplicateFieldIndex,
                "Field index " + std::to_string(field_idx) + " already exists");
        }
        throw GraphCoreError(
            GraphCoreErrorCode::InvalidFieldIndex,
            "Field index " + std::to_string(field_idx) + " is out of sequence; expected " +
                std::to_string(m_field_count));
    }

    // Add field to step's field list
    m_step_fields[step_idx].push_back(field_idx);

    // Track field properties
    m_field_owner_step.push_back(step_idx);
    m_field_types.push_back(ti);
    m_field_usages.push_back(usage);

    // Initialize union-find for this field (each field starts as its own root)
    m_field_uf_parent.push_back(field_idx);
    m_field_uf_rank.push_back(0);

    ++m_field_count;
}

// ============================================================================
// Step linking
// ============================================================================

void GraphCore::link_steps(size_t step_before_idx, size_t step_after_idx, TrustLevel trust)
{
    // Validate indices
    if (step_before_idx >= m_step_count)
    {
        throw GraphCoreError(
            GraphCoreErrorCode::InvalidStepIndex,
            "Before step index " + std::to_string(step_before_idx) + " does not exist");
    }
    if (step_after_idx >= m_step_count)
    {
        throw GraphCoreError(
            GraphCoreErrorCode::InvalidStepIndex,
            "After step index " + std::to_string(step_after_idx) + " does not exist");
    }

    // Self-loop check
    if (step_before_idx == step_after_idx)
    {
        throw GraphCoreError(
            GraphCoreErrorCode::CycleDetected,
            "Cannot link step " + std::to_string(step_before_idx) + " to itself");
    }

    // Store the link
    m_explicit_step_links.emplace_back(step_before_idx, step_after_idx);
    m_explicit_step_link_trust.push_back(trust);

    // Note: Cycle detection for non-trivial cycles is deferred to get_diagnostics()
    // unless eager validation is enabled. Full cycle detection requires graph traversal.
}

// ============================================================================
// Field linking
// ============================================================================

void GraphCore::link_fields(size_t field_one_idx, size_t field_two_idx, TrustLevel trust)
{
    // Validate indices
    if (field_one_idx >= m_field_count)
    {
        throw GraphCoreError(
            GraphCoreErrorCode::InvalidFieldIndex,
            "Field index " + std::to_string(field_one_idx) + " does not exist");
    }
    if (field_two_idx >= m_field_count)
    {
        throw GraphCoreError(
            GraphCoreErrorCode::InvalidFieldIndex,
            "Field index " + std::to_string(field_two_idx) + " does not exist");
    }

    // Self-link is a no-op
    if (field_one_idx == field_two_idx)
    {
        return;
    }

    // Type compatibility check
    if (m_field_types[field_one_idx] != m_field_types[field_two_idx])
    {
        throw GraphCoreError(
            GraphCoreErrorCode::TypeMismatch,
            "Cannot link fields with different types: field " +
                std::to_string(field_one_idx) + " (" +
                m_field_types[field_one_idx].name() + ") and field " +
                std::to_string(field_two_idx) + " (" +
                m_field_types[field_two_idx].name() + ")");
    }

    // Store the link edge
    m_field_links.emplace_back(field_one_idx, field_two_idx);
    m_field_link_trust.push_back(trust);

    // Unite equivalence classes
    uf_unite(field_one_idx, field_two_idx);

    // Note: Usage constraint validation (e.g., two Creates) is deferred to
    // get_diagnostics() where all fields in the equivalence class can be analyzed.
}

// ============================================================================
// Union-find helpers
// ============================================================================

FieldIdx GraphCore::uf_find(FieldIdx field_idx) const
{
    // Path compression: make every node point directly to the root
    if (m_field_uf_parent[field_idx] != field_idx)
    {
        m_field_uf_parent[field_idx] = uf_find(m_field_uf_parent[field_idx]);
    }
    return m_field_uf_parent[field_idx];
}

void GraphCore::uf_unite(FieldIdx field_a, FieldIdx field_b)
{
    FieldIdx root_a = uf_find(field_a);
    FieldIdx root_b = uf_find(field_b);

    if (root_a == root_b)
    {
        return; // Already in the same set
    }

    // Union by rank: attach smaller tree under root of larger tree
    if (m_field_uf_rank[root_a] < m_field_uf_rank[root_b])
    {
        m_field_uf_parent[root_a] = root_b;
    }
    else if (m_field_uf_rank[root_a] > m_field_uf_rank[root_b])
    {
        m_field_uf_parent[root_b] = root_a;
    }
    else
    {
        m_field_uf_parent[root_b] = root_a;
        ++m_field_uf_rank[root_a];
    }
}

// ============================================================================
// Diagnostics and export
// ============================================================================

std::shared_ptr<GraphCoreDiagnostics> GraphCore::get_diagnostics() const
{
    auto diagnostics = std::make_shared<GraphCoreDiagnostics>();

    // TODO: Implement full diagnostic analysis:
    // 1. Check for cycles in step ordering (explicit + implicit links)
    // 2. Check usage constraints per equivalence class (at most one Create, one Destroy)
    // 3. Check for orphan steps/fields
    // 4. Perform blame analysis using TrustLevel

    return diagnostics;
}

std::shared_ptr<ExportedGraph> GraphCore::export_graph() const
{
    // Check diagnostics first
    auto diagnostics = get_diagnostics();
    if (!diagnostics->is_valid())
    {
        throw GraphCoreError(
            GraphCoreErrorCode::InvalidState,
            "Cannot export graph with unresolved errors");
    }

    auto exported = std::make_shared<ExportedGraph>();

    // Build field-to-data mapping using union-find
    // Each equivalence class root becomes a data object index
    std::unordered_map<FieldIdx, DataIdx> root_to_data;
    DataIdx next_data_idx = 0;

    for (FieldIdx fidx = 0; fidx < m_field_count; ++fidx)
    {
        FieldIdx root = uf_find(fidx);
        auto it = root_to_data.find(root);
        if (it == root_to_data.end())
        {
            root_to_data[root] = next_data_idx;
            ++next_data_idx;
        }
        DataIdx didx = root_to_data[root];
        exported->field_data_pairs.emplace_back(fidx, didx);
    }

    // Build data_infos - use a map for construction since DataInfo has no default ctor
    std::unordered_map<DataIdx, DataInfo> data_info_map;

    for (FieldIdx fidx = 0; fidx < m_field_count; ++fidx)
    {
        FieldIdx root = uf_find(fidx);
        DataIdx didx = root_to_data[root];
        StepIdx sidx = m_field_owner_step[fidx];
        Usage usage = m_field_usages[fidx];

        auto it = data_info_map.find(didx);
        if (it == data_info_map.end())
        {
            // First field for this data object - use aggregate initialization
            DataInfo info{didx, m_field_types[fidx], {}};
            info.field_usages.emplace_back(sidx, fidx, usage);
            data_info_map.emplace(didx, std::move(info));
        }
        else
        {
            it->second.field_usages.emplace_back(sidx, fidx, usage);
        }
    }

    // Convert map to vector, sorted by DataIdx
    exported->data_infos.reserve(next_data_idx);
    for (DataIdx d = 0; d < next_data_idx; ++d)
    {
        exported->data_infos.push_back(std::move(data_info_map.at(d)));
    }

    // Copy explicit step links
    exported->explicit_step_links = m_explicit_step_links;

    // Build implicit step links from field usage ordering
    // For each data object, order steps by usage: Create < Read < Destroy
    for (const auto& data_info : exported->data_infos)
    {
        std::vector<StepIdx> create_steps;
        std::vector<StepIdx> read_steps;
        std::vector<StepIdx> destroy_steps;

        for (const auto& [sidx, fidx, usage] : data_info.field_usages)
        {
            switch (usage)
            {
            case Usage::Create:
                create_steps.push_back(sidx);
                break;
            case Usage::Read:
                read_steps.push_back(sidx);
                break;
            case Usage::Destroy:
                destroy_steps.push_back(sidx);
                break;
            }
        }

        // Create -> Read links
        for (StepIdx create_step : create_steps)
        {
            for (StepIdx read_step : read_steps)
            {
                if (create_step != read_step)
                {
                    exported->implicit_step_links.emplace_back(create_step, read_step);
                }
            }
        }

        // Create -> Destroy links
        for (StepIdx create_step : create_steps)
        {
            for (StepIdx destroy_step : destroy_steps)
            {
                if (create_step != destroy_step)
                {
                    exported->implicit_step_links.emplace_back(create_step, destroy_step);
                }
            }
        }

        // Read -> Destroy links
        for (StepIdx read_step : read_steps)
        {
            for (StepIdx destroy_step : destroy_steps)
            {
                if (read_step != destroy_step)
                {
                    exported->implicit_step_links.emplace_back(read_step, destroy_step);
                }
            }
        }
    }

    // Build combined links (union of explicit and implicit)
    exported->combined_step_links = exported->explicit_step_links;
    exported->combined_step_links.insert(
        exported->combined_step_links.end(),
        exported->implicit_step_links.begin(),
        exported->implicit_step_links.end());

    return exported;
}

} // namespace crddagt
