# 2026-01-09_153426_missing_create_seal_sensitivity.md

- Author: Claude Opus 4.5 (Anthropic)
- Date: 2026-01-09 (America/Los_Angeles)
- Document status: Frozen (defect resolved)
- Document type: Defect Analysis

## See Also

- [2026-01-08_180853_eager_cycle_detection_research.md](2026-01-08_180853_eager_cycle_detection_research.md) - Eager validation design (related context)
- [2026-01-08_094948_graph_core_design.md](2026-01-08_094948_graph_core_design.md) - Main GraphCore design document

## Summary

A latent architectural defect exists in the interaction between the old design (deferred validation), the new design (incremental eager validation), and the current implementation. The severity of the "missing Create" diagnostic is context-dependent on whether the graph is considered sealed, but neither `get_diagnostics()` nor the eager validation logic currently accounts for this.

## Background

### Old Design

The original design deferred all validation to `get_diagnostics()` and `export_graph()`. At the time these methods were called, the graph was implicitly considered complete—no further modifications were expected.

### New Design

The new design introduced eager validation during `link_steps()` and `link_fields()` to provide immediate feedback during incremental graph construction. This was motivated by the desire to catch errors as early as possible rather than discovering them only at export time.

### The Gap

The transition from "validate once at the end" to "validate incrementally during construction" exposed a semantic distinction that was previously implicit: **the graph's completeness state affects the interpretation of certain diagnostics**.

## The Defect

### Affected Diagnostic: Missing Create

Each data object (equivalence class of linked fields) must have exactly one Create field. The current implementation in `get_diagnostics()` treats a missing Create as an **Error**:

```
DiagnosticCategory::UsageConstraint
Severity: Error
Message: "Data object has no Create field"
```

### Context-Dependent Severity

The correct severity depends on whether the graph is considered sealed:

| Graph State | Missing Create Severity | Rationale |
|-------------|------------------------|-----------|
| **Open** (still being built) | Warning/Informational | The Create field may be added in a future call to `add_field()` and `link_fields()` |
| **Sealed** (construction complete) | Error | No more fields can be added; the missing Create is a genuine constraint violation |

### Current Behavior

- `get_diagnostics()`: Always reports missing Create as Error, regardless of graph state
- `export_graph()`: Calls `get_diagnostics()` and fails if `!is_valid()`, which includes the missing Create error

This means calling `get_diagnostics()` mid-construction will report false-positive errors for data objects whose Create fields have not yet been added.

## Design Constraints

### GraphCore Does Not Own Sealing

Per the existing architecture, GraphCore is a mutable builder. The concept of "sealing" (declaring the graph complete) is the upstream user's responsibility. GraphCore should not:

- Maintain internal sealed/unsealed state
- Provide a `seal()` method that prevents further mutations
- Make assumptions about when the user considers the graph complete

### GraphCore Must Respect Sealing Status

Although GraphCore does not own the sealing lifecycle, it must be informed of the sealing status in order to:

1. Correctly categorize the severity of context-dependent diagnostics
2. Distinguish between "not yet complete" (informational) and "incomplete" (error)

## Analysis of Affected Diagnostics

### Diagnostics That Are Seal-Insensitive

These diagnostics have the same severity regardless of graph state:

| Diagnostic | Category | Severity | Reason |
|------------|----------|----------|--------|
| Cycle detected | Cycle | Error | A cycle is always an error; adding more edges cannot remove a cycle |
| Multiple Creates | MultipleCreate | Error | Already have too many; adding more makes it worse |
| Multiple Destroys | MultipleDestroy | Error | Already have too many; adding more makes it worse |
| Unsafe self-aliasing | UnsafeSelfAliasing | Error | Incompatible usages on same step for same data |
| Type mismatch | TypeMismatch | Error | Type incompatibility is immediate and permanent |
| Unused data | UnusedData | Warning | Create without consumers is unusual but valid |
| Orphan step | OrphanStep | Warning | Step with no fields/links is unusual but valid |

### Diagnostics That Are Seal-Sensitive

MissingCreate is the only seal-sensitive diagnostic. All other diagnostics have fixed severity regardless of whether the graph is open or sealed.

| Diagnostic | Category | Open Severity | Sealed Severity | Reason |
|------------|----------|---------------|-----------------|--------|
| Missing Create | MissingCreate | Warning | Error | Create may be added later |

### MissingCreate Specification

**MissingCreate** applies to any equivalence class that contains Read or Destroy fields but no Create field. This includes:

- Singleton Read (one Read field, not linked to anything)
- Singleton Destroy (one Destroy field, not linked to anything)
- Linked Read + Destroy (no Create)
- Linked Read + Read (no Create)
- Any other combination lacking a Create

**Rationale**: A Read or Destroy field declares intent to access data. That data must be created by some Create field. The absence of Create means the data lifecycle is incomplete. Whether the field is linked to others (equivalence class size) is irrelevant to this constraint.

**Valid cases** (no MissingCreate error):

- Singleton Create: Data created, never consumed. Receives **UnusedData warning** (seal-insensitive).
- Create + Read: Normal data flow.
- Create + Destroy: Data created and destroyed.
- Create + Read + Destroy: Complete lifecycle.

> **Historical note**: This section was revised on 2026-01-09. The original document incorrectly stated that singleton Read/Destroy should receive an "OrphanField" warning instead of MissingCreate. This was based on a flawed condition `fields.size() > 1` that excluded singletons from MissingCreate detection. The `OrphanField` concept conflated linkage status with missing Create, which are distinct issues. See [2026-01-09_155955_orphan_field_conceptual_error.md](2026-01-09_155955_orphan_field_conceptual_error.md) for full analysis.

## Proposed Fix

### Option A: Parameter to `get_diagnostics()`

Add a parameter indicating whether the graph should be validated as sealed:

```cpp
std::shared_ptr<GraphCoreDiagnostics> get_diagnostics(bool treat_as_sealed = false) const;
```

- `treat_as_sealed = false`: Missing Create is Warning
- `treat_as_sealed = true`: Missing Create is Error

`export_graph()` would call `get_diagnostics(true)` since export implies completion.

**Pros**: Simple, explicit, no state to manage
**Cons**: Caller must remember to pass the correct flag

### Option B: Separate Severity in DiagnosticItem

Return both potential severities or a flag indicating seal-sensitivity:

```cpp
struct DiagnosticItem {
    DiagnosticSeverity severity;           // Severity when sealed
    DiagnosticSeverity open_severity;      // Severity when open (if different)
    bool seal_sensitive;                   // True if severities differ
    // ...
};
```

**Pros**: Full information available to caller
**Cons**: More complex API, caller must interpret

### Option C: Separate Diagnostic Categories

Treat "missing Create (open)" and "missing Create (sealed)" as distinct diagnostics:

- `DiagnosticCategory::IncompleteDataFlow` (Warning) - open graph
- `DiagnosticCategory::UsageConstraint` (Error) - sealed graph

**Pros**: Semantic clarity
**Cons**: Duplicates logic, complicates category enumeration

### Recommendation

**Option A** is recommended for its simplicity and alignment with the existing design principle that GraphCore does not own sealing state. The parameter makes the caller's intent explicit and avoids hidden state.

## Impact on Eager Validation

The eager validation in `link_fields()` currently checks for:

- Multiple Creates → throws `UsageConstraintViolation`
- Multiple Destroys → throws `UsageConstraintViolation`
- Self-aliasing → throws `UsageConstraintViolation`
- Cycle formation → throws `CycleDetected`

None of these are seal-sensitive; they are all immediate errors. The "missing Create" condition is not checked eagerly because:

1. It cannot be determined until the graph is sealed
2. Eagerly warning about it would produce noise during normal incremental construction

**No changes to eager validation are required.**

### Empirical Confirmation

Test case `UsageConstraint_MissingCreate_Eager` (added 2026-01-09) confirms:

1. `link_fields()` with eager validation does NOT throw when linking Read+Destroy (missing Create)
2. `get_diagnostics()` subsequently reports it as an Error

This demonstrates the defect: a caller using eager mode who calls `get_diagnostics()` mid-construction to check progress will receive a false-positive Error for data objects whose Create fields have not yet been added.

## Implementation Notes

1. Default `treat_as_sealed = false` preserves backward compatibility for callers who use `get_diagnostics()` mid-construction
2. `export_graph()` should pass `treat_as_sealed = true` to enforce complete validation
3. The diagnostic message for missing Create could include whether the graph was treated as sealed, to aid debugging
4. Documentation should clarify that `get_diagnostics()` results depend on the `treat_as_sealed` parameter

## Open Questions

### Q1: Should `export_graph()` have a "force" mode?

Should there be a way to export a graph with missing Creates (treating them as warnings)? Use case: exporting a partial graph for visualization or debugging.

**Tentative answer**: No. Export implies a complete, valid graph. For debugging, use `get_diagnostics(false)` to see warnings without blocking.

### Q2: Are there other seal-sensitive diagnostics?

This analysis identified only "missing Create" as seal-sensitive. Are there others that were overlooked?

**Tentative answer**: The current diagnostic categories appear complete. Future diagnostics should be evaluated for seal-sensitivity when added.

---

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-01-09 | Claude Opus 4.5 | Initial defect analysis document |
| 2026-01-10 | Claude Opus 4.5 | **Defect resolved**: `get_diagnostics(bool treat_as_sealed = false)` implemented. MissingCreate is Warning when unsealed, Error when sealed. 251 tests passing. |

---

*This document is Frozen. The defect has been resolved. See [2026-01-10_065043_diagnostic_revamp_mikado_plan.md](2026-01-10_065043_diagnostic_revamp_mikado_plan.md) for implementation details.*
