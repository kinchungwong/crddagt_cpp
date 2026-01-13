#include "crddagt/common/graph_builder.hpp"
#include "crddagt/common/vardata.inline.hpp"

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
    if (has_added_step)
    {
        /// @todo
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

} // namespace crddagt
