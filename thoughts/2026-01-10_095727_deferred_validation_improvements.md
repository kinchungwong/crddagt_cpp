# Deferred Validation Improvements

- **Status**: Frozen (implementation complete)
- **Author**: Claude Opus 4.5 (Anthropic)
- **Created**: 2026-01-10 (America/Los_Angeles)
- **Last Updated**: 2026-01-11

---

## Quick Reference

| Question | Answer |
|----------|--------|
| **What is this?** | Design and implementation for improving `GraphCore::get_diagnostics()` for graph in deferred validation mode |
| **What was done?** | Precise cycle reporting (Tarjan's SCC), implicit link blame with trust levels |
| **Is it complete?** | Yes (Phases 1-4). Phase 5 (deduplication) deferred as optional optimization. |
| **Key commit** | `997f73d` - Tarjan's algorithm for precise cycle detection |
| **Test file** | `tests/common/graph_core_diagnostics_tests.cpp` (55 tests) |

### When to Read This Document

- **You're debugging cycle detection** → Read "Code Analysis Findings" section
- **You're extending diagnostics** → Read "Detailed Design" section (D1, D2, D3)
- **You're reviewing test coverage** → Skip to "Test Case Inventory" at the end
- **You want the executive summary** → Read just this section and "Phase Status"

### Phase Status

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Iterative Tarjan's SCC helper | ✅ Complete |
| 2 | Integrate Tarjan into cycle reporting | ✅ Complete |
| 3 | Track implicit link trust levels | ✅ Complete |
| 4 | Blame implicit links | ✅ Complete |
| 5 | Deduplication (optional) | ⏸️ Deferred |

### Key Outcomes

- `get_diagnostics()` now uses Tarjan's SCC to identify cycle members precisely (not downstream steps)
- Implicit step links track trust levels derived from causing field links
- `blamed_field_links` is populated for implicit-only cycles (was previously empty)
- 289 total project tests passing (55 diagnostic tests)

---

## Related Documents

- [2026-01-10_065043_diagnostic_revamp_mikado_plan.md](2026-01-10_065043_diagnostic_revamp_mikado_plan.md) - Diagnostic revamp (prerequisite)
- [2026-01-08_180853_eager_cycle_detection_research.md](2026-01-08_180853_eager_cycle_detection_research.md) - Eager cycle detection
- [2026-01-08_094948_graph_core_design.md](2026-01-08_094948_graph_core_design.md) - Main GraphCore design

---

## Background

The recent work on GraphCore focused on **eager validation mode**:
- Eager cycle detection in `link_fields()` and `link_steps()`
- Immediate `MultipleCreate`, `MultipleDestroy`, `UnsafeSelfAliasing` throws
- Union-find with `IterableUnionFind` for efficient equivalence class operations

The **deferred validation mode** (`get_diagnostics()`) had quality gaps. Two improvement opportunities were identified:

1. **Cycle Reporting Precision**: Kahn's algorithm reported downstream tasks, not just cycle participants
2. **Implicit Link Blame**: Implicit-only cycles had empty blame lists

---

## Severity Assessment

| Issue | Severity | Resolution |
|-------|----------|------------|
| Downstream steps reported in cycles | **High** | ✅ Fixed (Tarjan's SCC) |
| Only explicit links blamed | **High** | ✅ Fixed (implicit blame) |
| No implicit trust level | **Medium** | ✅ Fixed (derived from field links) |
| No deduplication | **Low** | ⏸️ Deferred (performance only) |

---

## Code Analysis Findings

### Finding 1: No Deduplication (PERFORMANCE ISSUE, NOT BUG)

**Location**: `graph_core.cpp` lines 609-669

```cpp
// Line 609: Start with explicit links
std::vector<StepLinkPair> combined_links = m_explicit_step_links;

// Lines 635-668: Add implicit links WITHOUT checking for duplicates
for (StepIdx cs : create_steps)
{
    for (StepIdx rs : read_steps)
    {
        if (cs != rs)
        {
            combined_links.emplace_back(cs, rs);  // May duplicate explicit link!
        }
    }
}
```

**Analysis**: Kahn's algorithm is still correct because in-degree increments and decrements match. However, there's memory waste and extra iterations.

### Finding 2: Downstream Steps Reported (FIXED)

**Location**: `graph_core.cpp` lines 717-724

```cpp
// Collect steps involved in cycle (those with remaining in-degree > 0)
for (StepIdx s = 0; s < m_step_count; ++s)
{
    if (in_degree[s] > 0)
    {
        item.involved_steps.push_back(s);  // Includes ALL unreachable steps
    }
}
```

**Example**: For graph `A → B → C → B (cycle) → D → E`:
- Actual cycle: {B, C}
- Kahn reports: {B, C, D, E} (all steps with in_degree > 0)

**Fix**: Tarjan's SCC now identifies only actual cycle participants.

### Finding 3: Only Explicit Links Blamed (FIXED)

**Location**: `graph_core.cpp` lines 774-801 (`add_step_link_blame`)

The original code only searched `m_explicit_step_links`. Implicit-only cycles had empty blame.

**Fix**: Implicit links now track causing field links; these are added to `blamed_field_links`.

### Finding 4: Implicit Links Have No Trust Level (FIXED)

Implicit links are now assigned trust levels derived from the field links that cause them (minimum trust = most suspicious).

---

## Detailed Design

### Design D1: Precise Cycle Reporting with Tarjan's SCC

**Goal**: Replace the `in_degree > 0` step collection with Tarjan's SCC.

**Algorithm**:

```
1. After Kahn's algorithm detects a cycle (processed < m_step_count):
2. Build the "remaining subgraph":
   - Vertices: steps with in_degree > 0
   - Edges: combined_links where both endpoints have in_degree > 0
3. Run Tarjan's SCC on the remaining subgraph
4. Report only steps in SCCs with size > 1 (actual cycle participants)
5. Single-vertex SCCs are downstream dependencies, not cycle members
```

**Implementation**: Iterative Tarjan's SCC in anonymous namespace (no recursion for stack safety).

### Design D2: Implicit Link Blame with Derived Trust Levels

**Goal**: Blame implicit links when they participate in cycles.

**Trust Level Derivation**:

```
Implicit step link A→B caused by:
  - Field f1 (Usage::Create on step A) linked to field f2 (Usage::Read on step B)
  - Trust level = m_field_link_trust[link connecting f1↔f2]
```

If multiple field links cause the same implicit step link, use the **lowest** (most suspicious) trust level.

**Blame Output**: When blaming an implicit step link, add its `causing_field_links` to `blamed_field_links`.

### Design D3: Deduplication Strategy (DEFERRED)

**Goal**: Eliminate duplicate edges for performance.

**Implementation**: Use `unordered_set` to track existing edges during combined link building. Explicit links take priority for trust level.

**Status**: Not implemented. Performance impact is minor; algorithm correctness is unaffected.

---

## Implementation Plan (Mikado Style)

### Phase 1: Add Iterative Tarjan's SCC Helper ✅

Implement `find_strongly_connected_components()` helper function.

### Phase 2: Integrate Tarjan into Cycle Reporting ✅

Replace `in_degree > 0` collection with Tarjan's SCC. Only actual cycle participants reported.

### Phase 3: Track Implicit Link Trust Levels ✅

During implicit link generation, track causing field links. Compute trust as minimum of causing field link trust levels.

### Phase 4: Blame Implicit Links ✅

Search both explicit and implicit links. Add implicit link's causing field links to `blamed_field_links`.

### Phase 5: Deduplication ⏸️

Optional performance optimization. Deferred.

---

## Issue Details (Historical)

### Issue 1: Dependency Deduplication

The implementation maintains three sets of step dependencies:

| Set | Source | Storage |
|-----|--------|---------|
| Explicit step links | `link_steps()` | `m_explicit_step_links`, `m_explicit_step_link_trust` |
| Implicit step links | Derived from field usages | Computed on-demand |
| Combined links | Union of explicit + implicit | Computed on-demand in Kahn's algorithm |

**Potential issues identified**:
1. Duplicates across sets (explicit A→B may also be implicit)
2. Trust level conflicts for duplicate edges
3. Memory/performance overhead

**Resolution**: Deferred as low-priority performance optimization.

### Issue 2: Kahn's Algorithm Cycle Reporting

Kahn's algorithm identifies all steps that **cannot be reached** due to the cycle, not just the steps **in** the cycle.

**Improvement approaches considered**:

| Approach | Description | Complexity |
|----------|-------------|------------|
| **A: Tarjan's SCC** | Find strongly connected components | O(V+E) |
| **B: Johnson's algorithm** | Find all elementary cycles | O((V+E)(C+1)) |
| **C: Post-process Kahn** | DFS to find back-edges | O(V+E) additional |
| **D: Hybrid** | Kahn for detection, Tarjan for reporting | Two-phase |

**Chosen**: Hybrid (D) - Kahn detects, Tarjan reports precisely.

---

## Open Questions (Resolved)

### Q1: Is deduplication needed for correctness?

**ANSWERED: NO**. Kahn's algorithm is correct with duplicates. Deduplication is performance only.

### Q2: What is the implicit edge trust level?

**ANSWERED**: Derived from causing field links (minimum trust).

### Q3: Should cycle reporting change be breaking?

**ANSWERED**: Tests updated. More precise reporting is strictly better.

### Q4: Performance considerations?

**ANSWERED**: Tarjan's SCC is O(V+E), acceptable.

---

## Test Case Inventory

**File**: `tests/common/graph_core_diagnostics_tests.cpp`
**Total**: 55 tests in `GraphCoreDiagnosticsTests` suite

### Cycle Detection Tests (17 tests)

| Test Name | Description |
|-----------|-------------|
| `Cycle_SelfLoopExplicitStepLink` | Step linked to itself via link_steps() |
| `Cycle_TwoStepExplicitCycle_Eager` | A→B→A via explicit step links (eager mode) |
| `Cycle_TwoStepExplicitCycle_NonEager` | A→B→A via explicit step links (deferred mode) |
| `Cycle_ThreeStepExplicitCycle_Eager` | A→B→C→A via explicit step links (eager mode) |
| `Cycle_ThreeStepExplicitCycle_NonEager` | A→B→C→A via explicit step links (deferred mode) |
| `Cycle_ValidDAG_Eager` | Linear chain has no cycle (eager mode) |
| `Cycle_LongerCycle_Eager` | 4-step cycle (eager mode) |
| `Cycle_ImplicitFromUsageOrdering_NonEager` | Cycle from field usage ordering |
| `Cycle_ImplicitFromUsageOrdering_Eager` | Cycle from field usage ordering (eager mode) |
| `Cycle_ImplicitOnly_BlameNonEmpty_NonEager` | Implicit-only cycles have non-empty blame |
| `Cycle_MixedExplicitAndImplicit_NonEager` | Explicit + implicit edges forming cycle |
| `Cycle_MixedExplicitAndImplicit_Eager` | Explicit + implicit edges (eager mode) |
| `Cycle_NoCycleInValidDAG` | Valid DAG produces no cycle errors |
| `Cycle_ValidFieldLinks_Eager` | Valid field links don't cause false positives |
| `Cycle_BlameOrdersByTrustLevel_NonEager` | Lower trust links blamed first |
| `Cycle_PreciseReporting_ExcludesDownstream_NonEager` | Only cycle members reported, not downstream |
| `Cycle_PreciseReporting_MultipleDisjointCycles_NonEager` | Multiple SCCs correctly identified |

### Usage Constraint Tests (17 tests)

| Test Name | Description |
|-----------|-------------|
| `UsageConstraint_DoubleCreate_Eager` | Two Creates for same data (eager) |
| `UsageConstraint_DoubleCreate_NonEager` | Two Creates for same data (deferred) |
| `UsageConstraint_DoubleDestroy_Eager` | Two Destroys for same data (eager) |
| `UsageConstraint_DoubleDestroy_NonEager` | Two Destroys for same data (deferred) |
| `UsageConstraint_SelfAliasCreateAndRead_Eager` | Same step has Create and Read (eager) |
| `UsageConstraint_SelfAliasCreateAndRead_NonEager` | Same step has Create and Read (deferred) |
| `UsageConstraint_SelfAliasCreateAndDestroy_Eager` | Same step has Create and Destroy (eager) |
| `UsageConstraint_SelfAliasCreateAndDestroy_NonEager` | Same step has Create and Destroy (deferred) |
| `UsageConstraint_SelfAliasReadAndDestroy_NonEager` | Same step has Read and Destroy |
| `UsageConstraint_SelfAliasDoubleReadAllowed` | Multiple Reads allowed (no error) |
| `UsageConstraint_TransitiveDoubleCreate_Eager` | Transitive link connects two Creates (eager) |
| `UsageConstraint_TransitiveDoubleCreate_NonEager` | Transitive link connects two Creates (deferred) |
| `UsageConstraint_ValidCreateOnly` | Single Create - no error |
| `UsageConstraint_ValidCreateAndReads` | Create + Reads - no error |
| `UsageConstraint_ValidCreateReadsDestroy` | Full lifecycle - no error |
| `UsageConstraint_ValidCreateAndDestroy` | Create + Destroy (no Reads) - no error |
| `UsageConstraint_BlameOrdersByTrustLevel_NonEager` | Lower trust field links blamed first |

### Missing Create Tests (5 tests)

| Test Name | Description |
|-----------|-------------|
| `MissingCreate_Sealed_IsError` | Missing Create in sealed graph is error |
| `MissingCreate_Unsealed_IsWarning` | Missing Create in unsealed graph is warning |
| `MissingCreate_Eager_NotThrownDuringLinkFields` | Missing Create not checked eagerly |
| `MissingCreate_SingletonRead_Sealed` | Singleton Read with no Create |
| `MissingCreate_SingletonDestroy_Sealed` | Singleton Destroy with no Create |

### Orphan Step Tests (4 tests)

| Test Name | Description |
|-----------|-------------|
| `OrphanStep_NoFieldsNoLinks` | Step with no fields and no links → warning |
| `OrphanStep_HasFieldsNoLinks` | Step has fields → no warning |
| `OrphanStep_NoFieldsHasLinks` | Step has links → no warning |
| `OrphanStep_HasBoth` | Step has both → no warning |

### Unused Data Tests (3 tests)

| Test Name | Description |
|-----------|-------------|
| `UnusedData_SingletonCreate` | Create with no consumers |
| `UnusedData_LinkedCreateAndRead` | Create linked to Read - no warning |
| `UnusedData_FullLifecycle` | Full lifecycle - no warning |

### Diagnostics API Tests (5 tests)

| Test Name | Description |
|-----------|-------------|
| `API_EmptyGraphNoErrors` | Empty graph: is_valid() = true |
| `API_EmptyGraphNoWarnings` | Empty graph: has_warnings() = false |
| `API_ErrorMakesInvalid_NonEager` | Error makes is_valid() = false |
| `API_WarningStillValid` | Warning only: is_valid() = true |
| `API_AllItemsErrorsFirst_NonEager` | all_items() returns errors before warnings |

### Edge Case Tests (4 tests)

| Test Name | Description |
|-----------|-------------|
| `EdgeCase_EmptyGraph` | No steps, no fields → no diagnostics |
| `EdgeCase_SingleStepNoFields` | One step, no fields → orphan warning |
| `EdgeCase_SingleStepSingleField` | One step, one field → unused data warning |
| `EdgeCase_MultipleIndependentDataFlows` | Multiple independent flows validated |

---

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-01-10 | Claude Opus 4.5 | Initial document capturing improvement opportunities |
| 2026-01-10 | Claude Opus 4.5 | Code analysis complete: confirmed 4 issues |
| 2026-01-10 | Claude Opus 4.5 | Corrected Q1: deduplication is NOT a correctness bug |
| 2026-01-10 | Claude Opus 4.5 | Added detailed design: D1, D2, D3 |
| 2026-01-10 | Claude Opus 4.5 | Added 5-phase Mikado-style implementation plan |
| 2026-01-10 | Claude Opus 4.5 | Phases 1-4 complete. 254 tests passing. |
| 2026-01-11 | Claude Opus 4.5 | Document frozen. Reorganized (inverted pyramid). Fixed test counts. 289 tests. |
