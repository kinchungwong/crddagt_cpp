# 2026-01-10_065043_diagnostic_revamp_mikado_plan.md

- Author: Claude Opus 4.5 (Anthropic)
- Date: 2026-01-10 (America/Los_Angeles)
- Document status: Frozen (implementation complete)
- Document type: Mikado Plan

## See Also

- [2026-01-10_071521_dangers_of_invalid_graph_instantiation.md](2026-01-10_071521_dangers_of_invalid_graph_instantiation.md) - Dangers of invalid graph instantiation (extracted from this document)
- [2026-01-09_155955_orphan_field_conceptual_error.md](2026-01-09_155955_orphan_field_conceptual_error.md) - Defect analysis that motivated this plan
- [2026-01-09_153426_missing_create_seal_sensitivity.md](2026-01-09_153426_missing_create_seal_sensitivity.md) - Seal-sensitivity defect analysis
- [2026-01-08_180853_eager_cycle_detection_research.md](2026-01-08_180853_eager_cycle_detection_research.md) - Eager cycle detection (Phases 1-6 complete)

## Summary

This Mikado plan covers the diagnostic system revamp for GraphCore, including:
- Fixing the MissingCreate condition bug
- Removing OrphanField and adding new diagnostic categories
- Adding seal-sensitivity to `get_diagnostics()`
- Supporting forced export of invalid graphs with embedded diagnostics

---

## Decisions Record

### D1: Seal-Sensitivity Parameter (2026-01-10)

**Decision**: `get_diagnostics()` will accept a `bool treat_as_sealed` parameter, defaulting to `false`.

```cpp
std::shared_ptr<GraphCoreDiagnostics> get_diagnostics(bool treat_as_sealed = false) const;
```

- `treat_as_sealed = false` (default): MissingCreate is Warning
- `treat_as_sealed = true`: MissingCreate is Error

This is Option A from the 2026-01-09_153426 analysis.

### D2: OrphanField Removal (2026-01-10)

**Decision**: Remove `OrphanField` from `DiagnosticCategory`. The concept was flawed—it conflated linkage status with missing Create.

Replacement categories:
- **MissingCreate**: Handles singleton Read, singleton Destroy, linked Read+Destroy, etc.
- **UnusedData**: Handles singleton Create (data created but never consumed)

### D3: UsageConstraint Split (2026-01-10)

**Decision**: Split `UsageConstraint` into three specific categories:

| Old | New |
|-----|-----|
| UsageConstraint | MultipleCreate |
| UsageConstraint | MultipleDestroy |
| UsageConstraint | UnsafeSelfAliasing |

### D4: Forced Export of Invalid Graphs (2026-01-10)

**Decision**: Support forced export of invalid graphs for debugging and visualization purposes.

**Implementation approach**:
- `ExportedGraph` and individual records (links, data definitions) will include a `DiagnosticCategory` field
- This enables a second source of diagnostics in addition to `GraphCoreDiagnostics`
- Callers can inspect exported records for embedded error/warning markers

---

## Dangers of Invalid Graph Instantiation

> **Moved**: This section has been extracted into a dedicated reference document.
>
> See: [2026-01-10_071521_dangers_of_invalid_graph_instantiation.md](2026-01-10_071521_dangers_of_invalid_graph_instantiation.md)

The dangers document covers:
- Assumptions about checked data access layers
- Danger 1: Circular Ownership Memory Leak
- Danger 2: Unsatisfiable Tasks and Infinite Wait
- Danger 3: Access to Nonexistent Data
- Danger 4: Double Destroy
- Danger 5: Data Race from Multiple Create
- Implementation-specific consequence tables
- Summary table of mitigations

---

## Revised Diagnostic Categories

| Category | Description | Severity | Seal-Sensitive |
|----------|-------------|----------|----------------|
| Cycle | Cycle in step ordering | Error | No |
| MultipleCreate | More than one Create for same data | Error | No |
| MultipleDestroy | More than one Destroy for same data | Error | No |
| UnsafeSelfAliasing | Same step has incompatible usages for same data | Error | No |
| TypeMismatch | Linked fields have incompatible types | Error | No |
| MissingCreate | Read/Destroy without Create | Warning/Error | **Yes** |
| OrphanStep | Step with no fields and no links | Warning | No |
| UnusedData | Create without Read or Destroy | Warning | No |
| InternalError | Internal consistency error | Error | No |

---

## Mikado Goal Tree

```
[GOAL] Diagnostic System Revamp Complete ✓
├── [M1] DiagnosticCategory enum updated ✓
│   ├── [1.1] Remove OrphanField ✓
│   ├── [1.2] Add MissingCreate ✓
│   ├── [1.3] Add UnusedData ✓
│   ├── [1.4] Replace UsageConstraint with MultipleCreate ✓
│   ├── [1.5] Replace UsageConstraint with MultipleDestroy ✓
│   └── [1.6] Replace UsageConstraint with UnsafeSelfAliasing ✓
├── [M2] get_diagnostics() updated ✓
│   ├── [2.1] Add treat_as_sealed parameter ✓
│   ├── [2.2] Fix MissingCreate condition (remove fields.size() > 1) ✓
│   ├── [2.3] Implement UnusedData detection ✓
│   ├── [2.4] Update severity logic for MissingCreate ✓
│   └── [2.5] Remove OrphanField detection loop ✓
├── [M3] Eager validation updated ✓
│   ├── [3.1] Update link_fields() to throw MultipleCreate ✓
│   ├── [3.2] Update link_fields() to throw MultipleDestroy ✓
│   └── [3.3] Update link_fields() to throw UnsafeSelfAliasing ✓
├── [M4] ExportedGraph enhanced for forced export (DEFERRED)
│   ├── [4.1] Add DiagnosticCategory field to link records
│   ├── [4.2] Add DiagnosticCategory field to data definition records
│   ├── [4.3] Implement export_graph(bool force) or similar
│   └── [4.4] Document dangers of invalid graph instantiation ✓
├── [M5] Tests updated ✓
│   ├── [5.1] Singleton Read → MissingCreate ✓
│   ├── [5.2] Singleton Destroy → MissingCreate ✓
│   ├── [5.3] Singleton Create → UnusedData warning ✓
│   ├── [5.4] MissingCreate severity changes with seal state ✓
│   ├── [5.5] Update existing OrphanField tests ✓
│   ├── [5.6] Update existing UsageConstraint tests ✓
│   └── [5.7] Add forced export tests (DEFERRED with M4)
└── [M6] Documentation updated ✓
    ├── [6.1] Update graph_core_diagnostics.hpp comments ✓
    ├── [6.2] Update graph_core.hpp Doxygen ✓
    └── [6.3] Mark superseded sections in thought docs ✓
```

---

## Implementation Phases

### Phase 1: Update DiagnosticCategory Enum

**Milestone**: M1

**Changes to `graph_core_diagnostics.hpp`**:

```cpp
enum class DiagnosticCategory
{
    Cycle,
    MultipleCreate,      // was part of UsageConstraint
    MultipleDestroy,     // was part of UsageConstraint
    UnsafeSelfAliasing,  // was part of UsageConstraint
    MissingCreate,       // NEW: replaces partial OrphanField use
    TypeMismatch,
    OrphanStep,
    UnusedData,          // NEW: replaces partial OrphanField use
    InternalError
};
```

**Dependencies**: None (leaf node)

### Phase 2: Fix MissingCreate Condition

**Milestone**: M2.2

**Changes to `graph_core.cpp`**:

Change from:
```cpp
if (create_fields.empty() && fields.size() > 1)
```

To:
```cpp
if (create_fields.empty() && (!read_fields.empty() || !destroy_fields.empty()))
```

**Dependencies**: Phase 1 (needs MissingCreate category)

### Phase 3: Implement UnusedData Detection

**Milestone**: M2.3

**Logic**: Equivalence class has Create but no Read and no Destroy.

```cpp
if (!create_fields.empty() && read_fields.empty() && destroy_fields.empty())
{
    // UnusedData warning
}
```

**Dependencies**: Phase 1 (needs UnusedData category)

### Phase 4: Add Seal-Sensitivity Parameter

**Milestone**: M2.1, M2.4

**Changes**:
1. Add `bool treat_as_sealed = false` parameter to `get_diagnostics()`
2. MissingCreate severity = `treat_as_sealed ? Error : Warning`
3. Update `export_graph()` to call `get_diagnostics(true)`

**Dependencies**: Phase 2 (MissingCreate logic must be correct first)

### Phase 5: Remove OrphanField Detection

**Milestone**: M2.5, M1.1

**Changes**:
1. Remove OrphanField detection loop from `get_diagnostics()`
2. Remove `OrphanField` from enum (after tests updated)

**Dependencies**: Phases 2, 3 (replacement logic must be in place)

### Phase 6: Update Eager Validation Error Codes

**Milestone**: M3

**Changes to `link_fields()` in `graph_core.cpp`**:
- Change `UsageConstraintViolation` throws to specific codes:
  - Multiple Creates → `MultipleCreate`
  - Multiple Destroys → `MultipleDestroy`
  - Self-aliasing → `UnsafeSelfAliasing`

**Dependencies**: Phase 1 (needs new enum values)

### Phase 7: Enhance ExportedGraph for Forced Export

**Milestone**: M4

**Changes**:
1. Add `std::optional<DiagnosticCategory>` field to exported link/data records
2. Add `export_graph(bool force = false)` overload or separate method
3. When `force = true`, export even if `!is_valid()`, embedding diagnostics in records

**Dependencies**: Phases 1-5 (diagnostic system must be stable)

### Phase 8: Update Tests

**Milestone**: M5

**Test changes**:
1. Update `OrphanField_*` tests → expect `MissingCreate` or `UnusedData`
2. Update `UsageConstraint_*` tests → expect specific category
3. Add seal-sensitivity tests
4. Add forced export tests

**Dependencies**: Phases 1-7

### Phase 9: Documentation Cleanup

**Milestone**: M6

**Changes**:
1. Update Doxygen comments in headers
2. Mark superseded sections in thought documents
3. Update AGENTS.md if needed

**Dependencies**: Phase 8 (implementation complete)

---

## Affected Files

| File | Changes |
|------|---------|
| `graph_core_diagnostics.hpp` | New enum values, remove OrphanField |
| `graph_core_exceptions.hpp` | New error codes (or reuse existing) |
| `graph_core.cpp` | Fix MissingCreate, add UnusedData, update error codes, seal parameter |
| `graph_core.hpp` | Update get_diagnostics() signature |
| `exported_graph.hpp` | Add DiagnosticCategory to records |
| `exported_graph.cpp` | Implement forced export |
| `graph_core_diagnostics_tests.cpp` | Update all affected tests |

---

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-01-10 | Claude Opus 4.5 | Initial Mikado plan extracted from 2026-01-09_155955 |
| 2026-01-10 | Claude Opus 4.5 | Documented D1-D4 decisions |
| 2026-01-10 | Claude Opus 4.5 | Added "Dangers of Invalid Graph Instantiation" section |
| 2026-01-10 | Claude Opus 4.5 | Expanded implementation phases to 9 |
| 2026-01-10 | User | Added Assumptions subsection noting checked access layer mitigation |
| 2026-01-10 | Claude Opus 4.5 | Revised Dangers 3-5 with implementation-specific consequence tables |
| 2026-01-10 | Claude Opus 4.5 | Extracted dangers section to dedicated document |
| 2026-01-10 | Claude Opus 4.5 | **Implementation complete**: Phases 1-6, 8-9 done. M4 (forced export) deferred. 251 tests passing. |

---

*This document is Frozen. Implementation is complete except for the optional M4 (forced export) milestone.*
