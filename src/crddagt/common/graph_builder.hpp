/**
 * @file graph_builder.hpp
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/graph_core.hpp"
#include "crddagt/common/graph_items.hpp"
#include "crddagt/common/unique_shared_weak_list.hpp"
#include "crddagt/common/vardata.hpp"

namespace crddagt
{

class GraphBuilder
{
public:
    explicit GraphBuilder(bool eager_validation);

    void add_step(const StepPtr& step);
    void add_field(const FieldPtr& field);
    void link_steps(const StepPtr& before, const StepPtr& after, TrustLevel trust);
    void link_fields(const FieldPtr& field_one, const FieldPtr& field_two, TrustLevel trust);

private:
    bool m_eager_validation{};
    std::unique_ptr<GraphCore> m_core{};
    UniqueSharedWeakList<IStep> m_steps{};
    UniqueSharedWeakList<IField> m_fields{};
};

} // namespace crddagt
