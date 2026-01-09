# 2026-01-08_180853_eager_cycle_detection_research.md

- Author: Claude Opus 4.5 (Anthropic)
- Date: 2026-01-08 (America/Los_Angeles)
- Document status: Active

## See Also

- [2026-01-08_094948_graph_core_design.md](2026-01-08_094948_graph_core_design.md) - Main GraphCore design document (includes TrustLevel blame semantics for eager vs non-eager modes)
- [2026-01-08_185125_eager_cycle_detection_snippets.md](2026-01-08_185125_eager_cycle_detection_snippets.md) - Sample code snippets for this design
- [2026-01-08_202059_iterable_union_find_design.md](2026-01-08_202059_iterable_union_find_design.md) - Standalone IterableUnionFind class design (Implementation Plan Phases 1-3)

## Purpose

Research document for implementing eager cycle detection in `GraphCore`. When `eager_validation=true`, the system should detect and report cycles immediately upon the first action that induces a circular step dependency, rather than deferring detection to `get_diagnostics()`.

## Architectural Decisions

### AD1: Append-Only Design

**Decision**: The `GraphCore` system is **append-only** (additive-only) by design.

Once added, steps, fields, and links cannot be removed or modified. This architectural constraint enables:

1. **Eager cycle detection**: Cycles can be detected immediately upon adding an edge, since previously validated edges remain valid.
2. **Feasible algorithm choices**: Simple DFS reachability is sufficient; no need for complex incremental graph algorithms that handle deletions.
3. **Predictable performance**: No overhead from maintaining auxiliary structures for edge removal or invalidation tracking.
4. **Simplified reasoning**: The graph only grows; invariants established earlier remain true.

If future requirements demand mutability, this would require significant redesign of the validation strategy.

### AD2: `m_step_successors` Adjacency List

**Decision**: Implement `m_step_successors` as a forward adjacency list tracking both explicit and implicit step edges.

This decision is made irrespective of the pending open questions (Q1-Q3), as it is foundational infrastructure required for eager cycle detection. The adjacency list will be populated by:
- `link_steps()`: Adds explicit edges
- `link_fields()`: Adds implicit edges induced by Usage ordering

### AD3: Full Cross-Class Cycle Checking on Field Link

**Decision**: When `link_fields()` merges two equivalence classes, **all field pairs across the two classes** must be checked for induced implicit dependencies and cycle formation.

Checking only the directly linked pair is insufficient because a Read-Read link induces no direct dependency, yet merging the classes may bring Create and Destroy fields together, inducing new edges that could form cycles with existing explicit edges. See "Edge Cases and Considerations §1" for detailed rationale.

## Current State

### Existing Eager Validation

Currently, `GraphCore` with `eager_validation=true` performs the following checks immediately:

| Method | Eager Check | Throws |
|--------|-------------|--------|
| `add_step()` | Index sequentiality | `InvalidStepIndex`, `DuplicateStepIndex` |
| `add_field()` | Index sequentiality, step exists | `InvalidFieldIndex`, `DuplicateFieldIndex`, `InvalidStepIndex` |
| `link_steps()` | Self-loop only (`before == after`) | `CycleDetected` |
| `link_fields()` | Type mismatch | `TypeMismatch` |

### What's Missing

Eager mode does **not** currently detect:

1. **Multi-step cycles** from explicit step links (A→B→C→A)
2. **Cycles induced by implicit dependencies** from field usage ordering
3. **Mixed cycles** combining explicit and implicit dependencies

### Deferred Detection

Full cycle detection currently happens only in `get_diagnostics()` using Kahn's algorithm on the combined explicit + implicit step graph.

## Requirements for Eager Cycle Detection

### Semantics

1. **First action is the culprit**: In eager mode, the action that creates the cycle is reported as the culprit, regardless of TrustLevel
2. **TrustLevel still listed**: Affected edges include their TrustLevel for informational purposes
3. **Immediate exception**: Throw `GraphCoreError` with `CycleDetected` as soon as a cycle is formed

### Affected Methods

| Method | Cycle Detection Needed |
|--------|------------------------|
| `link_steps(before, after, trust)` | Check if adding `before→after` creates a cycle |
| `link_fields(field_a, field_b, trust)` | Check if induced implicit step dependencies create a cycle |

### Implicit Dependencies from Field Usage

When two fields are linked (declaring they reference the same data), implicit step dependencies are induced based on their `Usage` values:

| Field A Usage | Field B Usage | Induced Dependency |
|---------------|---------------|-------------------|
| Create | Read | step(A) → step(B) |
| Create | Destroy | step(A) → step(B) |
| Read | Destroy | step(A) → step(B) |
| Read | Create | step(B) → step(A) |
| Destroy | Create | step(B) → step(A) |
| Destroy | Read | step(B) → step(A) |
|---------------|---------------|-------------------|
| Read | Read | None *(see note 1)* |
|---------------|---------------|-------------------|
| Create | Create | Error *(see note 2)* |
| Destroy | Destroy | Error *(see note 2)* |

**Notes:**

1. **Read + Read**: No direct step dependency is induced between two Read fields. However, when `link_fields()` merges their equivalence classes, the merged class may contain Create or Destroy fields that induce dependencies with the Read fields. All cross-class implicit dependencies must be checked for cycles.

2. **Create + Create / Destroy + Destroy**: These are usage constraint violations (double Create or double Destroy for the same data object). They should throw `GraphCoreError` with `UsageConstraintViolation`, not `CycleDetected`. Usage constraint validation should occur before cycle detection.

## Proposed Approach: Targeted DFS from "After" Node

### Rationale

Full cycle detection on every mutation would be expensive (O(V+E) per operation). However, we can optimize based on the observation that:

1. Cycles can only form when adding a new edge `before→after`
2. A cycle exists if and only if there's already a path from `after` to `before` in the existing graph
3. We only need to search for a path from `after` back to `before`

### Algorithm: Reachability Check

For a new edge `before→after`:

```
function would_create_cycle(before, after):
    // Check if 'before' is reachable from 'after' in existing graph
    return is_reachable(from=after, to=before)
```

This is simpler than full Tarjan SCC - we just need a directed reachability query.

**Note on Tarjan SCC**: Since we are using simple DFS reachability (not computing strongly connected components), references to Tarjan's algorithm do not need to appear in actual source code. The possibility of using Tarjan SCC is reserved for future use if more sophisticated cycle analysis (e.g., enumerating all cycles, finding minimal cycle) becomes necessary.

### Implementation Options

*(See [snippets document](2026-01-08_185125_eager_cycle_detection_snippets.md) for sample code)*

#### Option A: Simple DFS Reachability

Iterative DFS from `from` node searching for `target`. Uses visited array to avoid cycles in the search itself.

**Complexity**: O(V+E) worst case, but typically much better due to early termination.

#### Option B: Bidirectional BFS

Search from both ends (`after` forward, `before` backward) and meet in the middle.

**Complexity**: O(√(V+E)) average case for random graphs, but graph structure may vary.

#### Option C: Incremental Transitive Closure

Maintain a transitive closure matrix updated on each edge addition.

**Complexity**: O(V²) space, O(V²) per update worst case. Not recommended for large graphs.

### Decision: Option A Selected

**Option A: Simple DFS Reachability** has been chosen for implementation.

Simple DFS is sufficient because:

1. **Typical graph structure**: Algorithm pipelines are mostly linear/DAG-like with limited branching
2. **Early termination**: We stop as soon as we find a path to `before`
3. **Locality**: As noted, recently added nodes tend to be near the end of the topological order, so searches from `after` are likely to explore a small subgraph

#### Optimization: Start from "After" Node

The user's insight is valuable:

> "By starting a search using the 'after' node at the most-recently-added before-after step dependency, hopefully the computational cost of the search would be manageable."

Since `after` is typically a recently-added node near the "end" of the partial DAG:
- It has few outgoing edges in the current graph
- The reachable set from `after` is likely small
- The search terminates quickly

## Implementation Design

*(See [snippets document](2026-01-08_185125_eager_cycle_detection_snippets.md) for sample code)*

### Data Structure: Forward Adjacency List

Add `m_step_successors` to `GraphCore` private members: a vector of vectors where `m_step_successors[s]` contains the list of steps that `s` has edges to.

Updated on:
- `link_steps()`: Add `after` to `m_step_successors[before]`
- `link_fields()`: Add induced implicit edges

### Data Structure: Equivalence Class Member List

To support O(class_size) enumeration of equivalence class members (required by AD3), extend the union-find structure with a circular linked list:

Add `m_field_uf_next` to `GraphCore` private members: a vector where `m_field_uf_next[i]` points to the next field in the same equivalence class, forming a circular list.

**Functions requiring changes in existing GraphCore**:

| Function | Change |
|----------|--------|
| `add_field()` | Initialize `m_field_uf_next[field_idx] = field_idx` |
| `uf_unite()` | Splice circular lists via `std::swap(m_field_uf_next[a], m_field_uf_next[b])` |

**New helper**: `get_equivalence_class(field)` iterates the circular list starting from `field`, returning all members in O(class_size).

### Modified `link_steps()`

1. Check for self-loop (`before == after`) — throw `CycleDetected`
2. If eager validation, call `is_reachable_from(after, before)` — if true, throw `CycleDetected`
3. Add edge to `m_explicit_step_links` and `m_step_successors`

### Modified `link_fields()`

1. Perform existing validation (type check)
2. Find equivalence class roots for both fields; if same root, return early (already linked)
3. If eager validation:
   - Collect all fields in each equivalence class via `get_equivalence_class()`
   - For each cross-class field pair, call `get_implicit_edge()` to determine induced dependency
   - For each induced edge, check `is_reachable_from(after, before)` — if cycle detected, throw `CycleDetected`
   - After all checks pass, add all new implicit edges to `m_step_successors`
4. Perform union-find merge and record field link

### Helper: `get_implicit_edge()`

**Precondition**: UsageConstraintViolation checks (multiple Create, multiple Destroy) have already been performed on the equivalence classes being merged. This helper is only called on field pairs that are known to be valid.

Compares `Usage` ordinal values (Create=0 < Read=1 < Destroy=2):
- If `usage_a < usage_b`: return `step_a → step_b`
- If `usage_b < usage_a`: return `step_b → step_a`
- If equal (Read-Read): return `nullopt` (no ordering induced)

**Note**: Create-Create and Destroy-Destroy pairs are never passed to this helper; they are caught earlier by the UsageConstraintViolation check.

### Helper: `is_reachable_from()`

Iterative DFS starting from `from`, searching for `target`. Returns `true` if `target` is reachable, `false` otherwise. Handles the trivial case `from == target` immediately.

## Edge Cases and Considerations

### 1. Transitive Field Links

**Decision**: When `link_fields(A, B)` is called and A and B belong to different equivalence classes, **all pairs of fields across the two classes must be checked** for induced implicit dependencies and cycle formation.

**Rationale**: Checking only the direct pair (A, B) is insufficient. Consider:
- Class 1: {Create@Step0, Read@Step2}
- Class 2: {Destroy@Step1, Read@Step3}
- New link: Read@Step2 to Read@Step3 (no direct dependency induced)

Although the direct pair (Read, Read) induces no dependency, merging these classes brings Create@Step0 and Destroy@Step1 into the same equivalence class. This induces Step0→Step1 (Create before Destroy). If an explicit edge Step1→Step0 already exists, a cycle is formed—but would be missed if only the direct pair were checked.

**Implementation requirement**: On `link_fields()`, enumerate all cross-class field pairs and check each induced implicit edge for cycle formation before completing the merge.

### 2. Multiple Implicit Edges from Single `link_fields()`

When linking fields in different equivalence classes, we may need to add multiple implicit edges. For example, if Class 1 contains `{Create@S0, Read@S1}` and Class 2 contains `{Read@S2, Destroy@S3}`, merging them induces three implicit edges: S0→S2, S0→S3, and S1→S3.

Each of these needs cycle checking.

### 3. Self-Aliasing Detection

If `link_fields()` links two fields on the same step with incompatible usages (Create+Read, Create+Destroy, Read+Destroy), this is a self-aliasing error, not a cycle. Should be caught separately.

### 4. Performance Considerations

| Scenario | Expected Performance |
|----------|---------------------|
| Small graph (<100 steps) | Negligible overhead |
| Linear pipeline | O(1) per check (after points to nothing) |
| Dense DAG | O(V+E) worst case per check |
| Adversarial input | Could be expensive |

**Mitigation**: For very large graphs, consider:
- Lazy validation mode
- Incremental topological ordering algorithms
    - Stratified (e.g. performed on high trust links first)
    - Intervaled (e.g. performed after adding a batch of links)
    - On-request
- Caching reachability results

## Open Questions

### Q1: Should we track implicit edges in `m_step_successors`?

**Pro**: Consistent cycle detection across explicit and implicit edges
**Con**: Implicit edges are derived, may change if fields are re-linked (not supported currently)

**Recommendation**: Yes, track them. GraphCore is append-only; fields cannot be unlinked.

### Q2: How to report the cycle path?

Currently `get_diagnostics()` reports involved steps. For eager mode:

**Option A**: Just report the new edge that triggered the cycle
**Option B**: Report the full cycle path (requires path reconstruction)

**Recommendation**: Option A for simplicity. The exception message states which edge was being added. Full path reconstruction can be added later if needed.

### Q3: Should eager validation be all-or-nothing?

Could have separate flags for:
- `eager_cycle_detection`
- `eager_usage_validation`
- `eager_orphan_warnings`

**Recommendation**: Keep single `eager_validation` flag for simplicity. Orphan detection naturally happens at `get_diagnostics()` time anyway.

## Implementation Plan

### Phase 1: Extract Iterable Union-Find Class

The embedded union-find implementation has grown into a full-featured component with:
- Union-by-rank
- Path compression (in `uf_find`)
- Exact size tracking with totality invariant
- Circular linked list for O(class_size) iteration

This warrants extraction into a standalone, reusable class for clarity and testability.

1. Create `iterable_union_find.hpp` with template class `IterableUnionFind<T>`
2. Encapsulate: parent, rank, size, and next arrays
3. Provide interface: `make_set()`, `find()`, `unite()`, `get_class_members()`, `class_size()`
4. Document thread-safety (externally synchronized, same as GraphCore)

### Phase 2: Union-Find Test Suite

Write comprehensive tests for the extracted union-find class to ensure correctness before integration.

1. Create `iterable_union_find_tests.cpp`
2. Test cases:
   - Singleton behavior (size=1, self-loop in next)
   - Two-element merge (both directions of rank)
   - Multi-element merge chains
   - `get_class_members()` returns all and only members
   - Size tracking: totality invariant holds after each operation
   - Path compression: `find()` flattens paths
   - Idempotent unite (same-class unite is no-op)

### Phase 3: Integrate Union-Find into GraphCore

1. Replace embedded union-find arrays with `IterableUnionFind<FieldIdx>` instance
2. Update `add_field()` to call `make_set()`
3. Update `link_fields()` to use new interface
4. Verify existing `GraphCoreDiagnosticsTests` still pass

### Phase 4: Step Successors Adjacency List

1. Add `m_step_successors` adjacency list to `GraphCore`
2. Implement `is_reachable_from()` helper (iterative DFS)
3. Update `add_step()` to resize `m_step_successors`

### Phase 5: Eager Cycle Detection in `link_steps()`

1. Add cycle check before adding edge
2. Update `m_step_successors` after successful add
3. Add unit tests for eager cycle detection

### Phase 6: Eager Cycle Detection in `link_fields()`

**Note**: At the start of `link_fields()`, the union-find methods `find()`, `class_size()`, and `get_class_members()` will be needed to obtain roots and enumerate both equivalence classes before any validation or merge occurs.

1. Implement `get_implicit_edge()` helper
2. **UsageConstraintViolation check first**: Before checking for cycles, verify that merging the two equivalence classes would not result in multiple Create or multiple Destroy fields for the same data object. Throw `UsageConstraintViolation` if violated.
3. Add cycle check for all cross-class induced edges (per AD3)
4. Update `m_step_successors` with implicit edges
5. Add unit tests

### Phase 7: Enhanced Error Reporting

1. Include affected edge TrustLevels in exception message
2. Consider path reconstruction for detailed error messages (optional)

## Test Cases Needed

### Eager Cycle Detection in `link_steps()`

| Test | Description |
|------|-------------|
| `Cycle_TwoStepExplicitCycle_Eager` | A→B then B→A throws immediately on second link |
| `Cycle_ThreeStepExplicitCycle_Eager` | A→B→C then C→A throws on third link |
| `Cycle_ValidDAG_Eager` | Valid DAG completes without exception |

### Eager Cycle Detection in `link_fields()`

| Test | Description |
|------|-------------|
| `Cycle_ImplicitFromUsageOrdering_Eager` | Field link induces cycle, throws immediately |
| `Cycle_MixedExplicitAndImplicit_Eager` | Explicit + implicit creates cycle |
| `Cycle_ValidFieldLinks_Eager` | Valid field linking completes without exception |

### Edge Cases

| Test | Description |
|------|-------------|
| `Cycle_SelfAliasIsNotCycle_Eager` | Self-aliasing throws UsageConstraint, not Cycle |
| `Cycle_TransitiveFieldMerge_Eager` | Merging equivalence classes with induced edges |

---

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-01-08 | Claude Opus 4.5 | Initial research document |
| 2026-01-08 | User + Claude | Clarified Read-Read, Create-Create, Destroy-Destroy cases in implicit dependency table |
| 2026-01-08 | Claude Opus 4.5 | Recorded decision: Option A (Simple DFS Reachability) selected; noted Tarjan SCC reserved for future use |
| 2026-01-08 | Claude Opus 4.5 | Split code snippets into separate document; kept research at high abstraction level |
| 2026-01-08 | Claude Opus 4.5 | Added AD1 (Append-Only Design) and AD2 (m_step_successors) architectural decisions |
| 2026-01-08 | Claude Opus 4.5 | Added AD3 (Full Cross-Class Cycle Checking); updated edge case 1 and implementation design |
| 2026-01-08 | Claude Opus 4.5 | Added equivalence class member list data structure (circular linked list for O(class_size) enumeration) |
| 2026-01-08 | Claude Opus 4.5 | Revised Implementation Plan: added phases for extracting IterableUnionFind class and test suite (Phases 1-3); renumbered subsequent phases |
| 2026-01-08 | Claude Opus 4.5 | Clarified Phase 6: UsageConstraintViolation check before cycle check; noted find/class_size/get_class_members needed at start of link_fields() |
| 2026-01-08 | Claude Opus 4.5 | Added precondition to get_implicit_edge() clarifying it's only called after UsageConstraintViolation checks pass |

---

*This document is Active and open for editing by both AI agents and humans.*
