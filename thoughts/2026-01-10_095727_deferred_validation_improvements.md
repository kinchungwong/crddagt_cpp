# 2026-01-10_095727_deferred_validation_improvements.md

- Author: Claude Opus 4.5 (Anthropic)
- Date: 2026-01-10 (America/Los_Angeles)
- Document status: Active
- Document type: Research / Planning

## See Also

- [2026-01-10_065043_diagnostic_revamp_mikado_plan.md](2026-01-10_065043_diagnostic_revamp_mikado_plan.md) - Recently completed diagnostic revamp
- [2026-01-08_180853_eager_cycle_detection_research.md](2026-01-08_180853_eager_cycle_detection_research.md) - Eager cycle detection (Phases 1-6 complete)
- [2026-01-08_094948_graph_core_design.md](2026-01-08_094948_graph_core_design.md) - Main GraphCore design document

## Context

The recent work on GraphCore focused intensely on **eager validation mode**:
- Eager cycle detection in `link_fields()` and `link_steps()`
- Immediate `MultipleCreate`, `MultipleDestroy`, `UnsafeSelfAliasing` throws
- Union-find with `IterableUnionFind` for efficient equivalence class operations

The **deferred validation mode** (`get_diagnostics()`) has received less attention and may have quality gaps. Two specific improvement opportunities have been identified:

1. **Dependency Deduplication**: Potential redundancy in implicit, explicit, and combined dependencies
2. **Cycle Reporting Precision**: Kahn's algorithm may report downstream tasks, not just cycle participants

---

## Issue 1: Dependency Deduplication

### Current State

The current implementation maintains three sets of step dependencies:

| Set | Source | Storage |
|-----|--------|---------|
| Explicit step links | `link_steps()` | `m_explicit_step_links`, `m_explicit_step_link_trust` |
| Implicit step links | Derived from field usages during `get_diagnostics()` | Computed on-demand |
| Combined links | Union of explicit + implicit | Computed on-demand in Kahn's algorithm |

### Potential Issues

1. **Duplicates across sets**: An explicit link A→B may also be implied by field usages. The combined set may contain duplicates.

2. **Trust level conflicts**: If A→B exists as both explicit (TrustLevel::High) and implicit (no trust level), which trust level applies for blame analysis?

3. **Memory/performance**: Redundant edges increase memory usage and iteration time in cycle detection.

### Questions to Investigate

- **Q1.1**: Does the current implementation deduplicate when building combined links?
- **Q1.2**: How are trust levels handled for edges that appear in both explicit and implicit sets?
- **Q1.3**: What are the performance implications of potential duplicates?
- **Q1.4**: Could deduplication introduce subtle bugs (e.g., losing blame information)?

### Deduplication Strategies

| Strategy | Pros | Cons |
|----------|------|------|
| **A: Dedupe at combine time** | Simple, no storage changes | Must still store duplicates until combine |
| **B: Prevent implicit if explicit exists** | Early deduplication | Requires checking explicit set during implicit derivation |
| **C: Store combined set incrementally** | Single source of truth | More complex invariants during construction |

---

## Issue 2: Kahn's Algorithm Cycle Reporting

### Current Behavior

The current cycle detection in `get_diagnostics()` uses Kahn's algorithm:

```
1. Build in-degree map for all steps
2. Initialize queue with steps having in-degree 0
3. Process queue, decrementing in-degrees
4. Steps remaining with in-degree > 0 are "involved" in the cycle
```

### The Problem

Kahn's algorithm identifies all steps that **cannot be reached** due to the cycle, not just the steps **in** the cycle.

**Example**:
```
A → B → C → D
    ↑       ↓
    └───────┘
```

- Actual cycle: B → C → D → B
- Kahn's algorithm reports: B, C, D (and A if it has no other path)
- If there are many downstream consumers of D, they are all reported

**Result**: The `involved_steps` list can be huge, making it difficult to identify the actual cycle edges.

### Current Trust-Based Blame

The diagnostic system has `blamed_step_links` ordered by trust level. However:

1. **All links in the "remaining" subgraph are blamed**, not just cycle edges
2. **Low trust doesn't help** if the cycle is entirely high-trust links
3. **No distinction** between cycle edges and downstream edges

### Improvement Approaches

| Approach | Description | Complexity |
|----------|-------------|------------|
| **A: Tarjan's SCC** | Find strongly connected components; report only SCC members | O(V+E), well-understood |
| **B: Johnson's algorithm** | Find all elementary cycles | O((V+E)(C+1)), C = number of cycles |
| **C: Post-process Kahn** | After Kahn, run DFS to find actual back-edges | O(V+E) additional |
| **D: Hybrid** | Use Kahn for detection, Tarjan for reporting | Two-phase, clear separation |

### Recommended Approach: Hybrid (D)

1. **Detection phase** (existing): Kahn's algorithm detects if a cycle exists
2. **Reporting phase** (new): If cycle detected, run Tarjan's SCC on the remaining subgraph
3. **Blame analysis**: Only edges within SCCs are blamed; edges to/from SCCs are secondary

### Benefits

- **Precise reporting**: Only actual cycle participants listed in `involved_steps`
- **Actionable blame**: Lower-trust edges within SCCs blamed first
- **Scalable**: Even with thousands of downstream steps, only cycle members reported

---

## Relationship to Trust Level System

Both issues relate to the trust level blame system:

| Issue | Trust Level Impact |
|-------|-------------------|
| Deduplication | If explicit and implicit edges coexist, which trust level wins? |
| Cycle reporting | Blame should focus on cycle edges, ordered by trust |

### Trust Level Design Principles

1. **Explicit links have explicit trust**: User specified it
2. **Implicit links have derived trust**: Based on field link trust that caused them
3. **Lower trust = more suspicious**: Blamed first in diagnostic output
4. **Deduplication should preserve lowest trust**: If same edge has multiple trust levels, keep the lowest (most suspicious)

---

## Proposed Investigation Plan

### Phase A: Code Analysis

1. Read current `get_diagnostics()` cycle detection code
2. Document how combined links are built
3. Identify if/where deduplication occurs
4. Trace how `blamed_step_links` is populated

### Phase B: Test Case Development

1. Create test case with explicit + implicit duplicate edge
2. Create test case with large downstream graph after cycle
3. Verify current behavior matches expectations (or document gaps)

### Phase C: Design Improvements

1. Design deduplication strategy
2. Design improved cycle reporting (likely Tarjan's SCC)
3. Document trust level handling for deduplicated edges

### Phase D: Implementation

1. Implement deduplication (if needed)
2. Implement precise cycle reporting
3. Update tests
4. Update documentation

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

**Analysis**: If explicit link A→B exists and implicit A→B is also added:
- `in_degree[B]` is incremented twice (2 instead of 1)
- `successors[A]` contains [B, B]
- When processing A, we decrement `in_degree[B]` twice (2→1→0)
- Step B is pushed to ready when in_degree reaches 0

**Kahn's algorithm is still correct** because the in-degree and decrement counts match. However:
- **Memory waste**: Duplicate edges stored
- **Extra iterations**: Processing duplicate successors
- **Conceptual concern**: The graph representation is denormalized

### Finding 2: Downstream Steps Reported (CONFIRMED DESIGN GAP)

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

**Impact**: Large downstream graphs produce huge `involved_steps` lists, making diagnosis difficult.

### Finding 3: Only Explicit Links Blamed (CONFIRMED DESIGN GAP)

**Location**: `graph_core.cpp` lines 774-801 (`add_step_link_blame`)

```cpp
// Only searches m_explicit_step_links
for (size_t i = 0; i < m_explicit_step_links.size(); ++i)
{
    const auto& [before, after] = m_explicit_step_links[i];
    if (step_set.count(before) > 0 && step_set.count(after) > 0)
    {
        blamed_with_trust.emplace_back(i, m_explicit_step_link_trust[i]);
    }
}
```

**Impact**: If a cycle is caused entirely by implicit links (field usage ordering), **no blame is assigned**. The `blamed_step_links` list will be empty.

### Finding 4: Implicit Links Have No Trust Level

Implicit links are not stored—they are computed on-demand. There is no mechanism to assign or track trust levels for implicit edges.

**Design question**: Should implicit link trust be:
- Inherited from the field links that cause them?
- A fixed default (e.g., `TrustLevel::High` since they're system-derived)?
- Tracked separately?

---

## Severity Assessment

| Issue | Severity | Impact |
|-------|----------|--------|
| No deduplication | **Low** | Performance: extra memory and iterations (algorithm still correct) |
| Downstream steps reported | **High** | Usability: huge output, impossible to diagnose large graphs |
| Only explicit links blamed | **High** | Usability: implicit-only cycles have NO blame (empty list) |
| No implicit trust level | **Medium** | Design gap: affects blame ordering for mixed cycles |

---

## Open Questions

### Q1: Is deduplication actually needed?

**ANSWERED: NO (for correctness)**. The current code does not deduplicate, but Kahn's algorithm is still correct because the in-degree increments and decrements match. If edge A→B appears twice, `in_degree[B]` is incremented twice (to 2), and when processing A, we decrement twice (2→1→0). Step B is pushed to ready when in_degree reaches 0, which is correct.

**However**, deduplication would improve:
- Memory usage (fewer duplicate edges stored)
- Iteration time (fewer successors to process)
- Conceptual clarity (normalized graph representation)

### Q2: What is the implicit edge trust level?

**ANSWERED: None**. Implicit edges have no trust level. This is a design gap that should be addressed.

### Q3: Should cycle reporting change be breaking?

Current tests may expect the current (imprecise) behavior. Need to audit tests.

### Q4: Performance considerations?

Tarjan's SCC is O(V+E), same as Kahn's. The additional pass is acceptable.

---

## Detailed Design

### Design D1: Precise Cycle Reporting with Tarjan's SCC

**Goal**: Replace the current `in_degree > 0` step collection with Tarjan's SCC to report only actual cycle participants.

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

**Implementation Approach**:

Implement a simple iterative Tarjan's SCC directly in `graph_core.cpp` (no recursion, to avoid stack overflow on large graphs). The demo_002 implementation uses recursion, but for crddagt_cpp (minimal dependencies), we need a self-contained iterative version.

**Data Structures**:

```cpp
struct TarjanState {
    size_t index_counter = 0;
    std::vector<size_t> index;       // Discovery index for each vertex
    std::vector<size_t> lowlink;     // Lowest reachable index
    std::vector<bool> on_stack;      // Whether vertex is on the stack
    std::vector<StepIdx> stack;      // DFS stack
    std::vector<std::vector<StepIdx>> sccs;  // Result: list of SCCs
};
```

**Expected Output Change**:

Before (current):
```
involved_steps: [B, C, D, E, F, G]  // Includes downstream steps
```

After (improved):
```
involved_steps: [B, C, D]  // Only actual cycle members
```

### Design D2: Implicit Link Blame with Derived Trust Levels

**Goal**: Blame implicit links (not just explicit) when they participate in cycles.

**Trust Level Derivation**:

Implicit step links are derived from field usages. The trust level should be inherited from the field links that caused the implicit dependency.

```
Implicit step link A→B caused by:
  - Field f1 (Usage::Create on step A) linked to field f2 (Usage::Read on step B)
  - Trust level = m_field_link_trust[link connecting f1↔f2]
```

**Challenge**: A single implicit step link may be caused by multiple field links (e.g., multiple Creates reading the same data). In this case, use the **lowest** (most suspicious) trust level.

**Implementation**:

```cpp
struct ImplicitStepLink {
    StepIdx before;
    StepIdx after;
    TrustLevel trust;
    std::vector<size_t> causing_field_links;  // For blame tracing
};
```

Build `implicit_step_links` during the existing implicit link generation loop, tracking which field links caused each implicit step link.

**Blame Output**:

```cpp
struct DiagnosticItem {
    // Existing:
    std::vector<size_t> blamed_step_links;    // Indices into explicit step links
    std::vector<size_t> blamed_field_links;   // Indices into field links

    // New (option A): Expand to include implicit links
    std::vector<ImplicitStepLink> blamed_implicit_step_links;

    // New (option B): Include causing field links in blamed_field_links
    // (implicit step links don't need separate storage - just blame their field causes)
};
```

**Recommendation**: Option B is simpler. When blaming an implicit step link, add its `causing_field_links` to `blamed_field_links`. The user can trace from field links back to the implicit ordering.

### Design D3: Deduplication Strategy

**Goal**: Avoid duplicate edges in combined_links for performance and clarity.

**Implementation**: Use an `unordered_set` to track existing edges:

```cpp
std::unordered_set<std::pair<StepIdx, StepIdx>, PairHash> edge_set;

// Add explicit links (first)
for (const auto& link : m_explicit_step_links) {
    edge_set.insert(link);
    combined_links.push_back(link);
}

// Add implicit links (skip if already exists)
for (StepIdx cs : create_steps) {
    for (StepIdx rs : read_steps) {
        if (cs != rs) {
            auto edge = std::make_pair(cs, rs);
            if (edge_set.insert(edge).second) {  // Returns false if already existed
                combined_links.push_back(edge);
            }
        }
    }
}
```

**Note**: This design choice means explicit links "win" over implicit links for trust level purposes. If A→B exists explicitly (TrustLevel::Low) and would also be implicit (from field usages), the explicit trust level is used for blame.

---

## Implementation Plan (Mikado Style)

### Phase 1: Add Iterative Tarjan's SCC Helper

**Goal**: Implement `find_strongly_connected_components()` helper function.

**Input**: Vertex count, edge list
**Output**: List of SCCs (each SCC is a list of vertex indices)

**Test**: Unit tests for Tarjan's SCC in isolation (new test file or section).

### Phase 2: Integrate Tarjan into Cycle Reporting

**Goal**: Replace `in_degree > 0` collection with Tarjan's SCC.

**Changes to `get_diagnostics()`**:
1. After Kahn detects cycle, extract remaining subgraph
2. Run Tarjan's SCC
3. Filter to SCCs with size > 1
4. Populate `involved_steps` with union of non-trivial SCCs

**Test**: Update existing cycle tests to verify precise reporting.

### Phase 3: Track Implicit Link Trust Levels

**Goal**: Derive trust levels for implicit step links.

**Changes**:
1. During implicit link generation, track causing field links
2. Compute trust level as minimum of causing field link trust levels
3. Store in new `ImplicitStepLink` structure (or inline)

**Test**: New tests for implicit link trust derivation.

### Phase 4: Blame Implicit Links

**Goal**: Include implicit links in cycle blame.

**Changes to `add_step_link_blame()`**:
1. Search both explicit and implicit links
2. Add implicit link's causing field links to `blamed_field_links`
3. Maintain trust-level ordering

**Test**: Tests for implicit-only cycles having non-empty blame.

### Phase 5: Deduplication (Optional)

**Goal**: Eliminate duplicate edges for performance.

**Changes**: Add `unordered_set` check during combined link building.

**Test**: Performance test with large graphs (optional).

---

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-01-10 | Claude Opus 4.5 | Initial document capturing improvement opportunities |
| 2026-01-10 | Claude Opus 4.5 | Code analysis complete: confirmed 4 issues (performance + 3 design gaps) |
| 2026-01-10 | Claude Opus 4.5 | Corrected Q1: deduplication is NOT a correctness bug, only performance |
| 2026-01-10 | Claude Opus 4.5 | Added detailed design: D1 (Tarjan's SCC), D2 (implicit blame), D3 (deduplication) |
| 2026-01-10 | Claude Opus 4.5 | Added 5-phase Mikado-style implementation plan |
| 2026-01-10 | Claude Opus 4.5 | **Phases 1-4 complete**: Tarjan's SCC, implicit link blame, precise cycle reporting. 254 tests passing. |

---

## Implementation Summary

### Completed (Phases 1-4)

1. **Phase 1**: Implemented iterative Tarjan's SCC algorithm in `graph_core.cpp` (anonymous namespace helper)
2. **Phase 2**: Integrated Tarjan's SCC into cycle reporting - only actual cycle participants are now reported
3. **Phase 3**: Added trust level tracking for implicit step links (derived from field link trust levels)
4. **Phase 4**: Modified blame analysis to include implicit links - their causing field links are now blamed

### Key Changes

- `get_diagnostics()` now uses Tarjan's SCC to identify cycle members precisely
- Implicit step links now track trust levels and causing field links
- `blamed_field_links` is populated for implicit-only cycles (was previously empty)
- Added 3 new tests: `Cycle_PreciseReporting_ExcludesDownstream`, `Cycle_PreciseReporting_MultipleDisjointCycles`, `Cycle_ImplicitOnly_BlameNonEmpty`

### Deferred (Phase 5)

Deduplication is optional since it's a performance optimization, not a correctness fix. The current implementation is correct but may have duplicate edges in combined_links.

---

*This document is Active but substantially complete. Phase 5 (deduplication) may be implemented in a future session.*
