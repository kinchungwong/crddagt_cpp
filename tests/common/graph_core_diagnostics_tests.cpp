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

TEST(GraphCoreDiagnosticsTests, Cycle_TwoStepExplicitCycle_Eager)
{
    // A→B and B→A creates a cycle - should throw immediately on second link
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.link_steps(0, 1, TrustLevel::Middle); // A→B succeeds

    // B→A should throw immediately
    EXPECT_THROW(graph.link_steps(1, 0, TrustLevel::Middle), GraphCoreError);
}

TEST(GraphCoreDiagnosticsTests, Cycle_ThreeStepExplicitCycle_Eager)
{
    // A→B→C then C→A creates a cycle - should throw on third link
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.link_steps(0, 1, TrustLevel::Middle); // A→B
    graph.link_steps(1, 2, TrustLevel::Middle); // B→C

    // C→A should throw immediately
    EXPECT_THROW(graph.link_steps(2, 0, TrustLevel::Middle), GraphCoreError);
}

TEST(GraphCoreDiagnosticsTests, Cycle_ValidDAG_Eager)
{
    // Valid DAG should not throw
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);

    // Build a valid DAG: A→B, A→C, B→C
    EXPECT_NO_THROW(graph.link_steps(0, 1, TrustLevel::High));
    EXPECT_NO_THROW(graph.link_steps(0, 2, TrustLevel::High));
    EXPECT_NO_THROW(graph.link_steps(1, 2, TrustLevel::High));
}

TEST(GraphCoreDiagnosticsTests, Cycle_LongerCycle_Eager)
{
    // A→B→C→D→E then E→A creates a cycle
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_step(3);
    graph.add_step(4);

    graph.link_steps(0, 1, TrustLevel::High);
    graph.link_steps(1, 2, TrustLevel::High);
    graph.link_steps(2, 3, TrustLevel::High);
    graph.link_steps(3, 4, TrustLevel::High);

    // E→A should throw immediately
    EXPECT_THROW(graph.link_steps(4, 0, TrustLevel::Low), GraphCoreError);
}

TEST(GraphCoreDiagnosticsTests, Cycle_TwoStepExplicitCycle_NonEager)
{
    // A→B and B→A creates a cycle
    GraphCore graph(false); // non-eager: cycle detected in get_diagnostics()
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

TEST(GraphCoreDiagnosticsTests, Cycle_ThreeStepExplicitCycle_NonEager)
{
    // A→B→C→A creates a cycle
    GraphCore graph(false); // non-eager: cycle detected in get_diagnostics()
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

TEST(GraphCoreDiagnosticsTests, Cycle_ImplicitFromUsageOrdering_NonEager)
{
    // Step 0: Create data D
    // Step 1: Destroy data D
    // Explicit link: Step 1 → Step 0 (Destroy before Create)
    // Implicit link: Step 0 → Step 1 (Create before Destroy)
    // This creates a cycle: 0→1 (implicit) and 1→0 (explicit)
    GraphCore graph(false); // non-eager: cycle detected in get_diagnostics()
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

TEST(GraphCoreDiagnosticsTests, Cycle_MixedExplicitAndImplicit_NonEager)
{
    // Step 0: Create data D
    // Step 1: Read data D
    // Explicit link: Step 1 → Step 0 (Read before Create)
    // Implicit link: Step 0 → Step 1 (Create before Read)
    // Cycle detected
    GraphCore graph(false); // non-eager: cycle detected in get_diagnostics()
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

TEST(GraphCoreDiagnosticsTests, Cycle_ImplicitFromUsageOrdering_Eager)
{
    // Step 0: Create data D
    // Step 1: Destroy data D
    // Explicit link: Step 1 → Step 0 (Destroy before Create)
    // Implicit link: Step 0 → Step 1 (Create before Destroy)
    // This creates a cycle - should throw on field link
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Destroy);
    graph.link_steps(1, 0, TrustLevel::Low); // Explicit: Destroy step before Create step

    // Linking fields induces implicit edge Create→Destroy (0→1)
    // Combined with explicit 1→0, this forms a cycle
    EXPECT_THROW(graph.link_fields(0, 1, TrustLevel::High), GraphCoreError);
}

TEST(GraphCoreDiagnosticsTests, Cycle_MixedExplicitAndImplicit_Eager)
{
    // Step 0: Create data D
    // Step 1: Read data D
    // Explicit link: Step 1 → Step 0
    // Implicit link: Step 0 → Step 1 (Create before Read)
    // Cycle detected on field link
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.link_steps(1, 0, TrustLevel::Low);

    EXPECT_THROW(graph.link_fields(0, 1, TrustLevel::High), GraphCoreError);
}

TEST(GraphCoreDiagnosticsTests, Cycle_ValidFieldLinks_Eager)
{
    // Valid data flow: Create → Read → Destroy
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(2, 2, typeid(int), Usage::Destroy);

    EXPECT_NO_THROW(graph.link_fields(0, 1, TrustLevel::High));
    EXPECT_NO_THROW(graph.link_fields(1, 2, TrustLevel::High));
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_DoubleCreate_Eager)
{
    // Two Create fields linked = error (eager mode throws immediately)
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Create);

    EXPECT_THROW(graph.link_fields(0, 1, TrustLevel::Middle), GraphCoreError);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_DoubleDestroy_Eager)
{
    // Two Destroy fields linked = error (eager mode throws immediately)
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Destroy);
    graph.add_field(2, 2, typeid(int), Usage::Destroy);

    graph.link_fields(0, 1, TrustLevel::Middle); // Create + Destroy OK
    EXPECT_THROW(graph.link_fields(1, 2, TrustLevel::Middle), GraphCoreError); // Second Destroy
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_SelfAliasCreateAndRead_Eager)
{
    // Same step has both Create and Read for same data = error
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(0, 1, typeid(int), Usage::Read);

    EXPECT_THROW(graph.link_fields(0, 1, TrustLevel::Middle), GraphCoreError);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_SelfAliasCreateAndDestroy_Eager)
{
    // Same step has both Create and Destroy for same data = error
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(0, 1, typeid(int), Usage::Destroy);

    EXPECT_THROW(graph.link_fields(0, 1, TrustLevel::Middle), GraphCoreError);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_TransitiveDoubleCreate_Eager)
{
    // A(Create) ↔ B(Read), then B ↔ C(Create) - transitively links two Creates
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.add_step(2);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(2, 2, typeid(int), Usage::Create);

    graph.link_fields(0, 1, TrustLevel::High); // Create ↔ Read OK
    EXPECT_THROW(graph.link_fields(1, 2, TrustLevel::High), GraphCoreError); // Merges classes, two Creates
}

TEST(GraphCoreDiagnosticsTests, Cycle_BlameOrdersByTrustLevel_NonEager)
{
    // Create a cycle with mixed trust levels
    // Lower trust links should appear first in blamed_step_links
    GraphCore graph(false); // non-eager: cycle detected in get_diagnostics()
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

TEST(GraphCoreDiagnosticsTests, UsageConstraint_DoubleCreate_NonEager)
{
    // Two Create fields linked to same data = error
    GraphCore graph(false); // non-eager: violation detected in get_diagnostics()
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Create);
    graph.link_fields(0, 1, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_multiple_create = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MultipleCreate)
        {
            found_multiple_create = true;
            break;
        }
    }
    EXPECT_TRUE(found_multiple_create);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_DoubleDestroy_NonEager)
{
    // Two Destroy fields linked to same data = error
    GraphCore graph(false); // non-eager: violation detected in get_diagnostics()
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

    bool found_multiple_destroy = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MultipleDestroy)
        {
            found_multiple_destroy = true;
            break;
        }
    }
    EXPECT_TRUE(found_multiple_destroy);
}

TEST(GraphCoreDiagnosticsTests, MissingCreate_Sealed_IsError)
{
    // Data with only Read/Destroy but no Create = error when sealed
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Read);
    graph.add_field(1, 1, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::Middle);

    // When sealed (treat_as_sealed=true), MissingCreate is an error
    auto diag = graph.get_diagnostics(true);
    EXPECT_TRUE(diag->has_errors());

    bool found_missing_create = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MissingCreate)
        {
            found_missing_create = true;
            EXPECT_EQ(item.severity, DiagnosticSeverity::Error);
            break;
        }
    }
    EXPECT_TRUE(found_missing_create);
}

TEST(GraphCoreDiagnosticsTests, MissingCreate_Unsealed_IsWarning)
{
    // Data with only Read/Destroy but no Create = warning when unsealed (default)
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Read);
    graph.add_field(1, 1, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::Middle);

    // When unsealed (treat_as_sealed=false), MissingCreate is a warning
    auto diag = graph.get_diagnostics(false);
    EXPECT_FALSE(diag->has_errors()) << "MissingCreate should be warning when unsealed";
    EXPECT_TRUE(diag->has_warnings());

    bool found_missing_create = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::MissingCreate)
        {
            found_missing_create = true;
            EXPECT_EQ(item.severity, DiagnosticSeverity::Warning);
            break;
        }
    }
    EXPECT_TRUE(found_missing_create);
}

TEST(GraphCoreDiagnosticsTests, MissingCreate_Eager_NotThrownDuringLinkFields)
{
    // MissingCreate is NOT checked eagerly during link_fields()
    // because the Create field might be added later
    GraphCore graph(true); // eager validation
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Read);
    graph.add_field(1, 1, typeid(int), Usage::Destroy);

    // link_fields() should NOT throw for MissingCreate
    EXPECT_NO_THROW(graph.link_fields(0, 1, TrustLevel::Middle));

    // When sealed, get_diagnostics(true) reports it as error
    auto diag = graph.get_diagnostics(true);
    EXPECT_TRUE(diag->has_errors()) << "Expected missing Create to be error when sealed";

    bool found_missing_create = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MissingCreate)
        {
            found_missing_create = true;
            break;
        }
    }
    EXPECT_TRUE(found_missing_create) << "Expected MissingCreate error when sealed";
}

TEST(GraphCoreDiagnosticsTests, MissingCreate_SingletonRead_Sealed)
{
    // Singleton Read (not linked to anything) = MissingCreate error when sealed
    // This is the bug fix: singleton Read was incorrectly getting OrphanField warning
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Read);

    // When sealed, singleton Read triggers MissingCreate error
    auto diag = graph.get_diagnostics(true);
    EXPECT_TRUE(diag->has_errors()) << "Singleton Read should be an error when sealed";

    bool found_missing_create = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MissingCreate)
        {
            found_missing_create = true;
            EXPECT_EQ(item.involved_fields.size(), 1u);
            EXPECT_EQ(item.involved_fields[0], 0u);
            break;
        }
    }
    EXPECT_TRUE(found_missing_create) << "Singleton Read should trigger MissingCreate error when sealed";
}

TEST(GraphCoreDiagnosticsTests, MissingCreate_SingletonDestroy_Sealed)
{
    // Singleton Destroy (not linked to anything) = MissingCreate error when sealed
    // This is the bug fix: singleton Destroy was incorrectly getting OrphanField warning
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Destroy);

    // When sealed, singleton Destroy triggers MissingCreate error
    auto diag = graph.get_diagnostics(true);
    EXPECT_TRUE(diag->has_errors()) << "Singleton Destroy should be an error when sealed";

    bool found_missing_create = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MissingCreate)
        {
            found_missing_create = true;
            EXPECT_EQ(item.involved_fields.size(), 1u);
            EXPECT_EQ(item.involved_fields[0], 0u);
            break;
        }
    }
    EXPECT_TRUE(found_missing_create) << "Singleton Destroy should trigger MissingCreate error when sealed";
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_SelfAliasCreateAndRead_NonEager)
{
    // Same step has both Create and Read for same data
    // This is self-aliasing: step reads data it also creates
    GraphCore graph(false); // non-eager: violation detected in get_diagnostics()
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(0, 1, typeid(int), Usage::Read);
    graph.link_fields(0, 1, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_self_aliasing = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UnsafeSelfAliasing)
        {
            found_self_aliasing = true;
            break;
        }
    }
    EXPECT_TRUE(found_self_aliasing);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_SelfAliasCreateAndDestroy_NonEager)
{
    // Same step has both Create and Destroy for same data
    GraphCore graph(false); // non-eager: violation detected in get_diagnostics()
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(0, 1, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_self_aliasing = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UnsafeSelfAliasing)
        {
            found_self_aliasing = true;
            break;
        }
    }
    EXPECT_TRUE(found_self_aliasing);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_SelfAliasReadAndDestroy_NonEager)
{
    // Same step has both Read and Destroy for same data
    GraphCore graph(false); // non-eager: violation detected in get_diagnostics()
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(1, 2, typeid(int), Usage::Destroy);
    graph.link_fields(0, 1, TrustLevel::Middle);
    graph.link_fields(1, 2, TrustLevel::Middle);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_errors());

    bool found_self_aliasing = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::UnsafeSelfAliasing)
        {
            found_self_aliasing = true;
            break;
        }
    }
    EXPECT_TRUE(found_self_aliasing);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_SelfAliasDoubleReadAllowed)
{
    // Same step has two Read fields for the same data
    // This should be allowed - a step can read the same data through multiple fields
    GraphCore graph(false);
    graph.add_step(0); // Creator
    graph.add_step(1); // Reader (reads same data twice)
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.add_field(1, 2, typeid(int), Usage::Read);
    graph.link_fields(0, 1, TrustLevel::High);
    graph.link_fields(1, 2, TrustLevel::High); // Both reads alias same data

    auto diag = graph.get_diagnostics();
    EXPECT_FALSE(diag->has_errors()) << "Double read on same step should not be an error";
    EXPECT_FALSE(diag->has_warnings()) << "Double read on same step should not be a warning";
    EXPECT_TRUE(diag->is_valid());
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_ValidCreateOnly)
{
    // Single Create field is valid
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);

    auto diag = graph.get_diagnostics();

    // No errors for MultipleCreate, MultipleDestroy, or UnsafeSelfAliasing
    bool found_usage_error = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MultipleCreate ||
            item.category == DiagnosticCategory::MultipleDestroy ||
            item.category == DiagnosticCategory::UnsafeSelfAliasing)
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

    // No errors for MultipleCreate, MultipleDestroy, or UnsafeSelfAliasing
    bool found_usage_error = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MultipleCreate ||
            item.category == DiagnosticCategory::MultipleDestroy ||
            item.category == DiagnosticCategory::UnsafeSelfAliasing)
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

    // No errors for MultipleCreate, MultipleDestroy, or UnsafeSelfAliasing
    bool found_usage_error = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MultipleCreate ||
            item.category == DiagnosticCategory::MultipleDestroy ||
            item.category == DiagnosticCategory::UnsafeSelfAliasing)
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

    // No errors for MultipleCreate, MultipleDestroy, or UnsafeSelfAliasing
    bool found_usage_error = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MultipleCreate ||
            item.category == DiagnosticCategory::MultipleDestroy ||
            item.category == DiagnosticCategory::UnsafeSelfAliasing)
        {
            found_usage_error = true;
            break;
        }
    }
    EXPECT_FALSE(found_usage_error);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_TransitiveDoubleCreate_NonEager)
{
    // A(Create) ↔ B(Read) ↔ C(Create)
    // Through transitivity, A and C are in same equivalence class
    // This means two Creates for same data = error
    GraphCore graph(false); // non-eager: violation detected in get_diagnostics()
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

    bool found_multiple_create = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MultipleCreate)
        {
            found_multiple_create = true;
            break;
        }
    }
    EXPECT_TRUE(found_multiple_create);
}

TEST(GraphCoreDiagnosticsTests, UsageConstraint_BlameOrdersByTrustLevel_NonEager)
{
    // Double create with mixed trust levels on field links
    GraphCore graph(false); // non-eager: violation detected in get_diagnostics()
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
        if (item.category == DiagnosticCategory::MultipleCreate)
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
// UnusedData Detection Tests (formerly OrphanField)
// ============================================================================

TEST(GraphCoreDiagnosticsTests, UnusedData_SingletonCreate)
{
    // Singleton Create = UnusedData warning (data created but never consumed)
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_field(0, 0, typeid(int), Usage::Create);

    auto diag = graph.get_diagnostics();
    EXPECT_TRUE(diag->has_warnings());

    bool found_unused = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::UnusedData)
        {
            found_unused = true;
            EXPECT_EQ(item.involved_fields.size(), 1u);
            EXPECT_EQ(item.involved_fields[0], 0u);
            break;
        }
    }
    EXPECT_TRUE(found_unused);
}

TEST(GraphCoreDiagnosticsTests, UnusedData_LinkedCreateAndRead)
{
    // Create linked to Read = NOT unused (has consumers)
    GraphCore graph(false);
    graph.add_step(0);
    graph.add_step(1);
    graph.add_field(0, 0, typeid(int), Usage::Create);
    graph.add_field(1, 1, typeid(int), Usage::Read);
    graph.link_fields(0, 1, TrustLevel::High);

    auto diag = graph.get_diagnostics();

    bool found_unused = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::UnusedData)
        {
            found_unused = true;
            break;
        }
    }
    EXPECT_FALSE(found_unused);
}

TEST(GraphCoreDiagnosticsTests, UnusedData_FullLifecycle)
{
    // Create + Read + Destroy = complete lifecycle, no UnusedData warning
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

    bool found_unused = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::UnusedData)
        {
            found_unused = true;
            break;
        }
    }
    EXPECT_FALSE(found_unused);
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

TEST(GraphCoreDiagnosticsTests, API_ErrorMakesInvalid_NonEager)
{
    // Create a cycle to generate an error
    GraphCore graph(false); // non-eager: cycle detected in get_diagnostics()
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

TEST(GraphCoreDiagnosticsTests, API_AllItemsErrorsFirst_NonEager)
{
    // Create both an error and a warning
    // Cycle error + orphan step in another step
    GraphCore graph(false); // non-eager: cycle detected in get_diagnostics()
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

    // Single Create field in singleton equivalence class = UnusedData warning
    EXPECT_TRUE(diag->is_valid());
    EXPECT_TRUE(diag->has_warnings());

    bool found = false;
    for (const auto& item : diag->warnings())
    {
        if (item.category == DiagnosticCategory::UnusedData)
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

    // Both flows are valid, no errors for MultipleCreate, MultipleDestroy, or UnsafeSelfAliasing
    bool found_usage_error = false;
    for (const auto& item : diag->errors())
    {
        if (item.category == DiagnosticCategory::MultipleCreate ||
            item.category == DiagnosticCategory::MultipleDestroy ||
            item.category == DiagnosticCategory::UnsafeSelfAliasing)
        {
            found_usage_error = true;
            break;
        }
    }
    EXPECT_FALSE(found_usage_error);
}
