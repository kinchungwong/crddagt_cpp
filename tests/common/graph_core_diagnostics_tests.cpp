/**
 * @file graph_core_diagnostics_tests.cpp
 * @brief Unit tests for GraphCore::get_diagnostics()
 *
 * TDD Red Phase: These tests define expected behavior for diagnostics.
 * All tests are expected to FAIL initially since get_diagnostics() returns empty.
 */
#include <gtest/gtest.h>
#include "crddagt/common/graph_core.hpp"

using namespace crddagt;

// ============================================================================
// Cycle Detection Tests
// ============================================================================

TEST(GraphCoreDiagnosticsTests, Cycle_SelfLoopExplicitStepLink)
{
    // Self-loop: step 0 linked to itself should be caught as a cycle
    // Note: link_steps() already throws for self-loops, so this tests
    // that diagnostics would report it if validation were deferred
    GraphCore graph(false); // lazy validation
    graph.add_step(0);

    // Self-loop is caught eagerly even with lazy validation
    EXPECT_THROW(graph.link_steps(0, 0, TrustLevel::Middle), GraphCoreError);
}

TEST(GraphCoreDiagnosticsTests, Cycle_TwoStepExplicitCycle)
{
    // A→B and B→A creates a cycle
    GraphCore graph(false); // lazy validation
    graph.add_step(0);
    graph.add_step(1);
    graph.link_steps(0, 1, TrustLevel::Middle);
    graph.link_steps(1, 0, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());
    EXPECT_FALSE(diag->is_valid());

    // Should have at least one cycle error
    bool found_cycle = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::Cycle)
        {
            found_cycle = true;
            // Both steps should be involved
            EXPECT_EQ(item.involved_steps.size(), 2u);
            break;
        }
    }
    EXPECT_TRUE(found_cycle);
}

TEST(GraphCoreDiagnosticsTests, Cycle_ThreeStepExplicitCycle)
{
    // A→B→C→A creates a cycle
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.link_steps(0, 1, TrustLevel::Middle);
    graph.link_steps(1, 2, TrustLevel::Middle);
    graph.link_steps(2, 0, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_cycle = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::Cycle)
        {
            found_cycle = true;
            EXPECT_EQ(item.involved_steps.size(), 3u);
            break;
        }
    }
    EXPECT_TRUE(found_cycle);
}

TEST(GraphCoreDiagnosticsTests, Cycle_ImplicitFromUsageOrdering)
{
    // Step 0: Create data D
    // Step 1: Destroy data D
    // Explicit link: Step 1 → Step 0 (Destroy before Create)
    // Implicit link: Step 0 → Step 1 (Create before Destroy)
    // This creates a cycle: 0→1 (implicit) and 1→0 (explicit)
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::High); // Same data
    graph.link_steps(1, 0, TrustLevel::Low);   // Explicit: Destroy before Create

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_cycle = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::Cycle)
        {
            found_cycle = true;
            break;
        }
    }
    EXPECT_TRUE(found_cycle);
}

TEST(GraphCoreDiagnosticsTests, Cycle_MixedExplicitAndImplicit)
{
    // Step 0: Create data D
    // Step 1: Read data D
    // Explicit link: Step 1 → Step 0 (Read before Create)
    // Implicit link: Step 0 → Step 1 (Create before Read)
    // Cycle detected
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.link_fields(0, 1, TrustLevel::High);
    graph.link_steps(1, 0, TrustLevel::Low);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_cycle = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::Cycle)
        {
            found_cycle = true;
            break;
        }
    }
    EXPECT_TRUE(found_cycle);
}

TEST(GraphCoreDiagnosticsTests, Cycle_NoCycleInValidDAG)
{
    // Linear chain: A→B→C (no cycle)
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.link_steps(0, 1, TrustLevel::High);
    graph.link_steps(1, 2, TrustLevel::High);

    auto diag = graph.get_diagnostics();

    // No cycle errors
    bool found_cycle = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::Cycle)
        {
            found_cycle = true;
            break;
        }
    }
    EXPECT_FALSE(found_cycle);
}

TEST(GraphCoreDiagnosticsTests, Cycle_BlameOrdersByTrustLevel)
{
    // Create a cycle with mixed trust levels
    // Lower trust links should appear first in blamed_step_links
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.link_steps(0, 1, TrustLevel::High); // index 0
    graph.link_steps(1, 0, TrustLevel::Low);  // index 1

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::Cycle)
        {
            // blamed_step_links should have Low trust link first
            ASSERT_GE(item.blamed_step_links.size(), 1u);
            // Index 1 (Low trust) should come before index 0 (High trust)
            EXPECT_EQ(item.blamed_step_links[0], 1u);
            break;
        }
    }
}

// ============================================================================
// Usage Constraint Tests
// ============================================================================

TEST(GraphCoreDiagnosticsTests, UsageConstraint_DoubleCreate)
{
    // Two Create fields linked to same data = error
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Create);
    graph.link_fields(0, 1, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_usage = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage = true;
            break;
        }
    }
    EXPECT_TRUE(found_usage);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_DoubleDestroy)
{
    // Two Destroy fields linked to same data = error
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Destroy);
    graph.add_field(2, 2, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::Middle);
    graph.link_fields(1, 2, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_usage = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage = true;
            break;
        }
    }
    EXPECT_TRUE(found_usage);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_MissingCreate)
{
    // Data with only Read/Destroy but no Create = error
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Read);
    graph.add_field(1, 1, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_usage = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage = true;
            break;
        }
    }
    EXPECT_TRUE(found_usage);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_SelfAliasCreateAndRead)
{
    // Same step has both Create and Read for same data
    // This is self-aliasing: step reads data it also creates
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(0, 1, typeid(int), Usage::Read);
    graph.link_fields(0, 1, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_usage = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage = true;
            break;
        }
    }
    EXPECT_TRUE(found_usage);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_SelfAliasCreateAndDestroy)
{
    // Same step has both Create and Destroy for same data
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(0, 1, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_usage = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage = true;
            break;
        }
    }
    EXPECT_TRUE(found_usage);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_SelfAliasReadAndDestroy)
{
    // Same step has both Read and Destroy for same data
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(1, 2, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::Middle);
    graph.link_fields(1, 2, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_usage = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage = true;
            break;
        }
    }
    EXPECT_TRUE(found_usage);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_ValidCreateOnly)
{
    // Single Create field is valid
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);

    auto diag = graph.get_diagnostics();

    bool found_usage_error = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage_error = true;
            break;
        }
    }
    EXPECT_FALSE(found_usage_error);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_ValidCreateAndReads)
{
    // Create + multiple Reads is valid
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(2, 2, typeid(int), Usage::Read);
    graph.link_fields(0, 1, TrustLevel::High);
    graph.link_fields(1, 2, TrustLevel::High);

    auto diag = graph.get_diagnostics();

    bool found_usage_error = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage_error = true;
            break;
        }
    }
    EXPECT_FALSE(found_usage_error);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_ValidCreateReadsDestroy)
{
    // Create + Reads + Destroy is valid
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(2, 2, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::High);
    graph.link_fields(1, 2, TrustLevel::High);

    auto diag = graph.get_diagnostics();

    bool found_usage_error = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage_error = true;
            break;
        }
    }
    EXPECT_FALSE(found_usage_error);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_ValidCreateAndDestroy)
{
    // Create + Destroy (no Reads) is valid
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::High);

    auto diag = graph.get_diagnostics();

    bool found_usage_error = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage_error = true;
            break;
        }
    }
    EXPECT_FALSE(found_usage_error);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_TransitiveDoubleCreate)
{
    // A(Create) ↔ B(Read) ↔ C(Create)
    // Through transitivity, A and C are in same equivalence class
    // This means two Creates for same data = error
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(2, 2, typeid(int), Usage::Create);
    graph.link_fields(0, 1, TrustLevel::High); // A ↔ B
    graph.link_fields(1, 2, TrustLevel::High); // B ↔ C (transitively A ↔ C)

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_usage = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage = true;
            break;
        }
    }
    EXPECT_TRUE(found_usage);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_BlameOrdersByTrustLevel)
{
    // Double create with mixed trust levels on field links
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(2, 2, typeid(int), Usage::Create);
    graph.link_fields(0, 1, TrustLevel::High); // index 0
    graph.link_fields(1, 2, TrustLevel::Low);  // index 1

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            // Low trust link (index 1) should be blamed first
            ASSERT_GE(item.blamed_field_links.size(), 1u);
            EXPECT_EQ(item.blamed_field_links[0], 1u);
            break;
        }
    }
}

// ============================================================================
// Orphan Step Detection Tests
// ============================================================================

TEST(GraphCoreDiagnosticsTests, OrphanStep_NoFieldsNoLinks)
{
    // Step with no fields and no links = orphan warning
    GraphCore graph(false);
    graph.add_step(0);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_warnings());

    bool found_orphan = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::OrphanStep)
        {
            found_orphan = true;
            EXPECT_EQ(item.involved_steps.size(), 1u);
            EXPECT_EQ(item.involved_steps[0], 0u);
            break;
        }
    }
    EXPECT_TRUE(found_orphan);
}

TEST(GraphCoreDiagnosticsTests, OrphanStep_HasFieldsNoLinks)
{
    // Step has fields but no explicit links = NOT orphan
    // (fields provide implicit dependencies)
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);

    auto diag = graph.get_diagnostics();

    bool found_orphan_step = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::OrphanStep)
        {
            found_orphan_step = true;
            break;
        }
    }
    EXPECT_FALSE(found_orphan_step);
}

TEST(GraphCoreDiagnosticsTests, OrphanStep_NoFieldsHasLinks)
{
    // Step has explicit links but no fields = NOT orphan
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.link_steps(0, 1, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();

    bool found_orphan_step = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::OrphanStep &&
            (item.involved_steps.size() == 1 && item.involved_steps[0] == 0))
        {
            found_orphan_step = true;
            break;
        }
    }
    // Step 0 has a link, so it's not orphan
    EXPECT_FALSE(found_orphan_step);
}

TEST(GraphCoreDiagnosticsTests, OrphanStep_HasBoth)
{
    // Step has both fields and links = NOT orphan
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.link_steps(0, 1, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();

    bool found_orphan_step = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::OrphanStep &&
            (item.involved_steps.size() == 1 && item.involved_steps[0] == 0))
        {
            found_orphan_step = true;
            break;
        }
    }
    EXPECT_FALSE(found_orphan_step);
}

// ============================================================================
// Orphan Field Detection Tests
// ============================================================================

TEST(GraphCoreDiagnosticsTests, OrphanField_SingletonEquivalenceClass)
{
    // Field not linked to any other = orphan warning
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_warnings());

    bool found_orphan = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::OrphanField)
        {
            found_orphan = true;
            EXPECT_EQ(item.involved_fields.size(), 1u);
            EXPECT_EQ(item.involved_fields[0], 0u);
            break;
        }
    }
    EXPECT_TRUE(found_orphan);
}

TEST(GraphCoreDiagnosticsTests, OrphanField_LinkedToOthers)
{
    // Field linked to other fields = NOT orphan
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.link_fields(0, 1, TrustLevel::High);

    auto diag = graph.get_diagnostics();

    bool found_orphan_field = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::OrphanField)
        {
            found_orphan_field = true;
            break;
        }
    }
    EXPECT_FALSE(found_orphan_field);
}

TEST(GraphCoreDiagnosticsTests, OrphanField_AllFieldsLinked)
{
    // All fields in one equivalence class = no orphan warnings
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(2, 2, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::High);
    graph.link_fields(1, 2, TrustLevel::High);

    auto diag = graph.get_diagnostics();

    bool found_orphan_field = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::OrphanField)
        {
            found_orphan_field = true;
            break;
        }
    }
    EXPECT_FALSE(found_orphan_field);
}

// ============================================================================
// Diagnostics API Tests
// ============================================================================

TEST(GraphCoreDiagnosticsTests, API_EmptyGraphNoErrors)
{
    GraphCore graph(false);
    auto diag = graph.get_diagnostics();

    EXPECT_FALSE(diag->has_errors());
    EXPECT_TRUE(diag->is_valid());
    EXPECT_TRUE(diag->errors().empty());
}

TEST(GraphCoreDiagnosticsTests, API_EmptyGraphNoWarnings)
{
    GraphCore graph(false);
    auto diag = graph.get_diagnostics();

    EXPECT_FALSE(diag->has_warnings());
    EXPECT_TRUE(diag->warnings().empty());
}

TEST(GraphCoreDiagnosticsTests, API_ErrorMakesInvalid)
{
    // Create a cycle to generate an error
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.link_steps(0, 1, TrustLevel::Middle);
    graph.link_steps(1, 0, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();

    EXPECT_TRUE(diag->has_errors());
    EXPECT_FALSE(diag->is_valid());
}

TEST(GraphCoreDiagnosticsTests, API_WarningStillValid)
{
    // Create an orphan step to generate a warning (but no error)
    GraphCore graph(false);
    graph.add_step(0);

    auto diag = graph.get_diagnostics();

    // Should have warning but still be valid
    EXPECT_TRUE(diag->has_warnings());
    EXPECT_TRUE(diag->is_valid());
}

TEST(GraphCoreDiagnosticsTests, API_AllItemsErrorsFirst)
{
    // Create both an error and a warning
    // Cycle error + orphan step in another step
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2); // orphan step
    graph.link_steps(0, 1, TrustLevel::Middle);
    graph.link_steps(1, 0, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    auto all = diag->all_items();

    // Should have at least one error and one warning
    EXPECT_TRUE(diag->has_errors());
    EXPECT_TRUE(diag->has_warnings());

    // All errors should come before all warnings
    bool seen_warning = false;
    for (const auto& item : all)
    {
        if (item.severity == DiagnosticSeverity::Warning)
        {
            seen_warning = true;
        }
        else if (item.severity == DiagnosticSeverity::Error)
        {
            // Should not see an error after a warning
            EXPECT_FALSE(seen_warning);
        }
    }
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST(GraphCoreDiagnosticsTests, EdgeCase_EmptyGraph)
{
    GraphCore graph(false);
    auto diag = graph.get_diagnostics();

    // Empty graph: no errors, no warnings
    EXPECT_TRUE(diag->is_valid());
    EXPECT_FALSE(diag->has_errors());
    EXPECT_FALSE(diag->has_warnings());
}

TEST(GraphCoreDiagnosticsTests, EdgeCase_SingleStepNoFields)
{
    GraphCore graph(false);
    graph.add_step(0);

    auto diag = graph.get_diagnostics();

    // Single step with no fields = orphan step warning
    EXPECT_TRUE(diag->is_valid()); // warnings don't block
    EXPECT_TRUE(diag->has_warnings());

    bool found = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::OrphanStep)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(GraphCoreDiagnosticsTests, EdgeCase_SingleStepSingleField)
{
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);

    auto diag = graph.get_diagnostics();

    // Single field in singleton equivalence class = orphan field warning
    EXPECT_TRUE(diag->is_valid());
    EXPECT_TRUE(diag->has_warnings());

    bool found = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::OrphanField)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(GraphCoreDiagnosticsTests, EdgeCase_MultipleIndependentDataFlows)
{
    // Two separate data flows, each valid
    // Data flow 1: step 0 creates, step 1 reads
    // Data flow 2: step 2 creates, step 3 destroys
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_step(3);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(2, 2, typeid(double), Usage::Create);
    graph.add_field(3, 3, typeid(double), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::High); // Flow 1
    graph.link_fields(2, 3, TrustLevel::High); // Flow 2

    auto diag = graph.get_diagnostics();

    // Both flows are valid, no errors
    bool found_usage_error = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UsageConstraint)
        {
            found_usage_error = true;
            break;
        }
    }
    EXPECT_FALSE(found_usage_error);
}
