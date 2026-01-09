/**
 * @file graph_core.cpp
 */
#include "crddagt/common/graph_core.hpp"

#include <algorithm>
#include <queue>
#include <unordered_set>

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

    // =========================================================================
    // Phase 1: Build equivalence classes and collect field info per data object
    // =========================================================================

    // Map from root field index to list of (field_idx, step_idx, usage)
    std::unordered_map<FieldIdx, std::vector<std::tuple<FieldIdx, StepIdx, Usage>>> equiv_classes;

    for (FieldIdx fidx = 0; fidx < m_field_count; ++fidx)
    {
        FieldIdx root = uf_find(fidx);
        equiv_classes[root].emplace_back(fidx, m_field_owner_step[fidx], m_field_usages[fidx]);
    }

    // =========================================================================
    // Phase 2: Usage constraint validation per equivalence class
    // =========================================================================

    for (const auto& [root, fields] : equiv_classes)
    {
        std::vector<FieldIdx> create_fields;
        std::vector<FieldIdx> destroy_fields;
        std::vector<FieldIdx> read_fields;
        std::unordered_map<StepIdx, std::vector<std::pair<FieldIdx, Usage>>> step_usages;

        for (const auto& [fidx, sidx, usage] : fields)
        {
            step_usages[sidx].emplace_back(fidx, usage);

            switch (usage)
            {
            case Usage::Create:
                create_fields.push_back(fidx);
                break;
            case Usage::Read:
                read_fields.push_back(fidx);
                break;
            case Usage::Destroy:
                destroy_fields.push_back(fidx);
                break;
            }
        }

        // Check: Multiple Creates
        if (create_fields.size() > 1)
        {
            DiagnosticItem item;
            item.severity = DiagnosticSeverity::Error;
            item.category = DiagnosticCategory::UsageConstraint;
            item.message = "Multiple Create fields for same data object";
            item.involved_fields = create_fields;
            // Collect involved steps
            for (FieldIdx f : create_fields)
            {
                item.involved_steps.push_back(m_field_owner_step[f]);
            }
            // Blame analysis: find field links involved, order by trust
            add_field_link_blame(item, create_fields);
            diagnostics->m_errors.push_back(std::move(item));
        }

        // Check: Multiple Destroys
        if (destroy_fields.size() > 1)
        {
            DiagnosticItem item;
            item.severity = DiagnosticSeverity::Error;
            item.category = DiagnosticCategory::UsageConstraint;
            item.message = "Multiple Destroy fields for same data object";
            item.involved_fields = destroy_fields;
            for (FieldIdx f : destroy_fields)
            {
                item.involved_steps.push_back(m_field_owner_step[f]);
            }
            add_field_link_blame(item, destroy_fields);
            diagnostics->m_errors.push_back(std::move(item));
        }

        // Check: Missing Create (only if there are other usages - linked fields)
        if (create_fields.empty() && fields.size() > 1)
        {
            DiagnosticItem item;
            item.severity = DiagnosticSeverity::Error;
            item.category = DiagnosticCategory::UsageConstraint;
            item.message = "Data object has no Create field";
            for (const auto& [fidx, sidx, usage] : fields)
            {
                item.involved_fields.push_back(fidx);
                item.involved_steps.push_back(sidx);
            }
            // Blame all field links in this equivalence class
            std::vector<FieldIdx> all_fields;
            for (const auto& [fidx, sidx, usage] : fields)
            {
                all_fields.push_back(fidx);
            }
            add_field_link_blame(item, all_fields);
            diagnostics->m_errors.push_back(std::move(item));
        }

        // Check: Self-aliasing (same step has incompatible usages for same data)
        // Multiple Reads on the same step for the same data are allowed.
        // Disallowed combinations: Create+Read, Create+Destroy, Read+Destroy
        for (const auto& [sidx, usages] : step_usages)
        {
            if (usages.size() > 1)
            {
                // Check if all usages are Read - that's allowed
                bool all_reads = true;
                for (const auto& [fidx, usage] : usages)
                {
                    if (usage != Usage::Read)
                    {
                        all_reads = false;
                        break;
                    }
                }

                if (!all_reads)
                {
                    // Mixed usages = self-aliasing error
                    DiagnosticItem item;
                    item.severity = DiagnosticSeverity::Error;
                    item.category = DiagnosticCategory::UsageConstraint;
                    item.message = "Self-aliasing: step " + std::to_string(sidx) +
                                   " has incompatible field usages for same data object";
                    item.involved_steps.push_back(sidx);
                    for (const auto& [fidx, usage] : usages)
                    {
                        item.involved_fields.push_back(fidx);
                    }
                    add_field_link_blame(item, item.involved_fields);
                    diagnostics->m_errors.push_back(std::move(item));
                }
            }
        }
    }

    // =========================================================================
    // Phase 3: Orphan detection
    // =========================================================================

    // Orphan steps: steps with no fields AND no explicit links
    std::vector<bool> step_has_link(m_step_count, false);
    for (const auto& [before, after] : m_explicit_step_links)
    {
        step_has_link[before] = true;
        step_has_link[after] = true;
    }

    for (StepIdx sidx = 0; sidx < m_step_count; ++sidx)
    {
        bool has_fields = !m_step_fields[sidx].empty();
        bool has_links = step_has_link[sidx];

        if (!has_fields && !has_links)
        {
            DiagnosticItem item;
            item.severity = DiagnosticSeverity::Warning;
            item.category = DiagnosticCategory::OrphanStep;
            item.message = "Step " + std::to_string(sidx) + " has no fields and no links";
            item.involved_steps.push_back(sidx);
            diagnostics->m_warnings.push_back(std::move(item));
        }
    }

    // Orphan fields: fields in singleton equivalence class (not linked to any other)
    for (const auto& [root, fields] : equiv_classes)
    {
        if (fields.size() == 1)
        {
            const auto& [fidx, sidx, usage] = fields[0];
            DiagnosticItem item;
            item.severity = DiagnosticSeverity::Warning;
            item.category = DiagnosticCategory::OrphanField;
            item.message = "Field " + std::to_string(fidx) + " is not linked to any other field";
            item.involved_fields.push_back(fidx);
            item.involved_steps.push_back(sidx);
            diagnostics->m_warnings.push_back(std::move(item));
        }
    }

    // =========================================================================
    // Phase 4: Cycle detection using Kahn's algorithm
    // =========================================================================

    // Build combined step links (explicit + implicit)
    std::vector<StepLinkPair> combined_links = m_explicit_step_links;

    // Add implicit links from usage ordering
    for (const auto& [root, fields] : equiv_classes)
    {
        std::vector<StepIdx> create_steps;
        std::vector<StepIdx> read_steps;
        std::vector<StepIdx> destroy_steps;

        for (const auto& [fidx, sidx, usage] : fields)
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

        // Create -> Read
        for (StepIdx cs : create_steps)
        {
            for (StepIdx rs : read_steps)
            {
                if (cs != rs)
                {
                    combined_links.emplace_back(cs, rs);
                }
            }
        }

        // Create -> Destroy
        for (StepIdx cs : create_steps)
        {
            for (StepIdx ds : destroy_steps)
            {
                if (cs != ds)
                {
                    combined_links.emplace_back(cs, ds);
                }
            }
        }

        // Read -> Destroy
        for (StepIdx rs : read_steps)
        {
            for (StepIdx ds : destroy_steps)
            {
                if (rs != ds)
                {
                    combined_links.emplace_back(rs, ds);
                }
            }
        }
    }

    // Kahn's algorithm for cycle detection
    if (m_step_count > 0)
    {
        std::vector<size_t> in_degree(m_step_count, 0);
        std::vector<std::vector<StepIdx>> successors(m_step_count);

        for (const auto& [before, after] : combined_links)
        {
            successors[before].push_back(after);
            ++in_degree[after];
        }

        std::queue<StepIdx> ready;
        for (StepIdx s = 0; s < m_step_count; ++s)
        {
            if (in_degree[s] == 0)
            {
                ready.push(s);
            }
        }

        size_t processed = 0;
        while (!ready.empty())
        {
            StepIdx s = ready.front();
            ready.pop();
            ++processed;

            for (StepIdx succ : successors[s])
            {
                --in_degree[succ];
                if (in_degree[succ] == 0)
                {
                    ready.push(succ);
                }
            }
        }

        // If not all steps were processed, there's a cycle
        if (processed < m_step_count)
        {
            DiagnosticItem item;
            item.severity = DiagnosticSeverity::Error;
            item.category = DiagnosticCategory::Cycle;
            item.message = "Cycle detected in step ordering";

            // Collect steps involved in cycle (those with remaining in-degree > 0)
            for (StepIdx s = 0; s < m_step_count; ++s)
            {
                if (in_degree[s] > 0)
                {
                    item.involved_steps.push_back(s);
                }
            }

            // Blame analysis: find explicit step links involved in cycle
            add_step_link_blame(item, item.involved_steps);

            diagnostics->m_errors.push_back(std::move(item));
        }
    }

    return diagnostics;
}

void GraphCore::add_field_link_blame(DiagnosticItem& item,
                                      const std::vector<FieldIdx>& involved_fields) const
{
    // Find field links that connect any of the involved fields
    std::unordered_set<FieldIdx> field_set(involved_fields.begin(), involved_fields.end());

    std::vector<std::pair<size_t, TrustLevel>> blamed_with_trust;

    for (size_t i = 0; i < m_field_links.size(); ++i)
    {
        const auto& [f1, f2] = m_field_links[i];
        // Check if this link connects two fields in the involved set
        // or connects a field in the set to another in the same equivalence class
        FieldIdx root1 = uf_find(f1);
        FieldIdx root2 = uf_find(f2);
        bool f1_involved = field_set.count(f1) > 0;
        bool f2_involved = field_set.count(f2) > 0;

        // If either endpoint is involved
        if (f1_involved || f2_involved)
        {
            blamed_with_trust.emplace_back(i, m_field_link_trust[i]);
        }
    }

    // Sort by trust level (Low first, then Middle, then High)
    std::sort(blamed_with_trust.begin(), blamed_with_trust.end(),
              [](const auto& a, const auto& b) {
                  return static_cast<int>(a.second) < static_cast<int>(b.second);
              });

    for (const auto& [idx, trust] : blamed_with_trust)
    {
        item.blamed_field_links.push_back(idx);
    }
}

void GraphCore::add_step_link_blame(DiagnosticItem& item,
                                     const std::vector<StepIdx>& involved_steps) const
{
    // Find explicit step links that connect any of the involved steps
    std::unordered_set<StepIdx> step_set(involved_steps.begin(), involved_steps.end());

    std::vector<std::pair<size_t, TrustLevel>> blamed_with_trust;

    for (size_t i = 0; i < m_explicit_step_links.size(); ++i)
    {
        const auto& [before, after] = m_explicit_step_links[i];
        // Link is involved if both endpoints are in the cycle
        if (step_set.count(before) > 0 && step_set.count(after) > 0)
        {
            blamed_with_trust.emplace_back(i, m_explicit_step_link_trust[i]);
        }
    }

    // Sort by trust level (Low first, then Middle, then High)
    std::sort(blamed_with_trust.begin(), blamed_with_trust.end(),
              [](const auto& a, const auto& b) {
                  return static_cast<int>(a.second) < static_cast<int>(b.second);
              });

    for (const auto& [idx, trust] : blamed_with_trust)
    {
        item.blamed_step_links.push_back(idx);
    }
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
