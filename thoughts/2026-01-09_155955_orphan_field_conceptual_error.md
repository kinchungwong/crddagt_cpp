# 2026-01-09_155955_orphan_field_conceptual_error.md

- Author: Claude Opus 4.5 (Anthropic)
- Date: 2026-01-09 (America/Los_Angeles)
- Document status: Active
- Document type: Defect Analysis (supersedes portions of 2026-01-09_153426)

## See Also

- [2026-01-09_153426_missing_create_seal_sensitivity.md](2026-01-09_153426_missing_create_seal_sensitivity.md) - Related defect (seal-sensitivity); this document supersedes its analysis of OrphanField
- [2026-01-08_094948_graph_core_design.md](2026-01-08_094948_graph_core_design.md) - Main GraphCore design document

## Summary

The `OrphanField` diagnostic category is based on a conceptual error that conflates two distinct concepts:

1. **Being unlinked** (a singleton equivalence class) - not inherently an error
2. **Missing Create** (data accessed without being created) - a genuine constraint violation

This has led to incorrect diagnostic behavior where singleton Read or Destroy fields receive a misleading "orphan" warning instead of the correct "missing Create" diagnostic.

## The Conceptual Error

### Flawed Mental Model

The current design treats "being unlinked" as the problem:

```
OrphanField: "A field is not linked to any data flow"
```

This implies that singleton fields are problematic because they're isolated. But this conflates linkage status with semantic validity.

### Correct Mental Model

The actual invariant is about data lifecycle, not linkage:

| Field Usage | Without Create | Validity |
|-------------|----------------|----------|
| Create | N/A (is the Create) | Valid - data created |
| Read | Missing Create | Invalid - can't read nonexistent data |
| Destroy | Missing Create | Invalid - can't destroy nonexistent data |

Being a singleton is orthogonal to validity:

| Scenario | Singleton? | Valid? | Reason |
|----------|------------|--------|--------|
| Create alone | Yes | **Valid** | Data created, never consumed (unusual but legal) |
| Read alone | Yes | **Invalid** | Can't read what doesn't exist |
| Destroy alone | Yes | **Invalid** | Can't destroy what doesn't exist |
| Create + Read | No | **Valid** | Normal data flow |
| Read + Destroy | No | **Invalid** | Missing Create |
| Create + Read + Destroy | No | **Valid** | Complete data lifecycle |

### The Faulty Condition

In `graph_core.cpp` lines 479-480:

```cpp
// Check: Missing Create (only if there are other usages - linked fields)
if (create_fields.empty() && fields.size() > 1)
```

The `fields.size() > 1` condition was intended to exclude "orphan" singletons. But this incorrectly allows singleton Read and singleton Destroy to escape the MissingCreate check, receiving only an "orphan" warning.

## Misleading Documentation

### In graph_core_diagnostics.hpp (line 33)

```cpp
OrphanField,            ///< A field is not linked to any data flow.
```

**Problem**: This suggests linkage is the issue. A singleton Create IS a valid data flow (data created, lifecycle complete).

### In 2026-01-09_153426_missing_create_seal_sensitivity.md (lines 102-104)

> "This is correct behavior: a singleton Create field is valid (the data is created but never used). A singleton Read or Destroy field is an orphan warning, not a missing-Create error, because there's no evidence it's part of a multi-field data flow."

**Problem**: The second sentence is incorrect. A singleton Read or Destroy IS evidence of a missing Create. The field declares intent to access data; that data must have a Create somewhere.

### In 2026-01-08_094948_graph_core_design.md

The design lists `OrphanField` as a diagnostic category without clarifying that it's about unused data, not missing Creates.

## Correct Diagnostic Model

### Remove or Redefine OrphanField

**Option A: Remove OrphanField entirely**

- MissingCreate handles Read/Destroy without Create
- Singleton Create is valid (no diagnostic needed)
- OrphanStep is sufficient for step-level warnings

**Option B: Redefine OrphanField as "UnusedData"**

- Rename to `UnusedData` or `DataNeverConsumed`
- Applies only to equivalence classes with Create but no Read or Destroy
- Severity: Warning (informational - might be intentional)

### Fix MissingCreate Condition

Change from:
```cpp
if (create_fields.empty() && fields.size() > 1)
```

To:
```cpp
if (create_fields.empty() && !fields.empty())
```

Or more explicitly:
```cpp
// Any equivalence class with Read or Destroy but no Create
if (create_fields.empty() && (!read_fields.empty() || !destroy_fields.empty()))
```

This ensures:
- Singleton Read → MissingCreate
- Singleton Destroy → MissingCreate
- Linked Read+Destroy → MissingCreate
- Singleton Create → No error (valid)
- Empty equivalence class → No error (shouldn't happen)

### Integrate Seal-Sensitivity

Per the analysis in 2026-01-09_153426, MissingCreate severity depends on seal state:

| Graph State | MissingCreate Severity |
|-------------|------------------------|
| Open | Warning |
| Sealed | Error |

## Revised Diagnostic Categories

| Category | Description | Severity | Seal-Sensitive |
|----------|-------------|----------|----------------|
| Cycle | Cycle in step ordering | Error | No |
| UsageConstraint | Multiple Creates, multiple Destroys, self-aliasing | Error | No |
| TypeMismatch | Linked fields have incompatible types | Error | No |
| MissingCreate | Read/Destroy without Create | Warning (open) / Error (sealed) | **Yes** |
| OrphanStep | Step with no fields and no links | Warning | No |
| UnusedData | Create without Read or Destroy | Warning | No |
| InternalError | Internal consistency error | Error | No |

**Notes**:
- `OrphanField` is removed (replaced by correct use of MissingCreate and UnusedData)
- `UnusedData` applies to singleton Create or any equivalence class with Create but no Read/Destroy
- `MissingCreate` is the only seal-sensitive diagnostic

## Implementation Plan

### Phase 1: Fix MissingCreate Condition

1. Change condition from `fields.size() > 1` to check for Read/Destroy presence
2. Remove or update the misleading comment

### Phase 2: Handle OrphanField Category

**If removing:**
1. Remove `OrphanField` from `DiagnosticCategory` enum
2. Remove orphan field detection loop from `get_diagnostics()`
3. Update tests to expect MissingCreate instead of OrphanField for singleton Read/Destroy

**If renaming to UnusedData:**
1. Rename enum value
2. Change detection logic to: Create exists AND no Read AND no Destroy
3. Update tests

### Phase 3: Add Seal-Sensitivity

1. Add `treat_as_sealed` parameter to `get_diagnostics()`
2. Set MissingCreate severity based on parameter
3. Update `export_graph()` to pass `true`

### Phase 4: Test Coverage

Ensure tests for:
- Singleton Create → Valid (no error, possibly UnusedData warning)
- Singleton Read → MissingCreate
- Singleton Destroy → MissingCreate
- Linked Read+Destroy → MissingCreate
- Linked Create+Read → Valid
- MissingCreate severity changes with seal state

## Affected Files

| File | Changes |
|------|---------|
| `graph_core_diagnostics.hpp` | Update/remove `OrphanField` enum |
| `graph_core.cpp` | Fix MissingCreate condition, update orphan field logic |
| `graph_core_diagnostics_tests.cpp` | Update tests for new behavior |
| `2026-01-09_153426_*.md` | Mark superseded sections |

## Resolved Questions

### Q1: Should we warn about unused data (Create without consumers)?

A singleton Create or Create-only equivalence class means data is created but never read or destroyed. This could be:
- Intentional (side effects, external consumption)
- A mistake (forgot to add Read/Destroy)

**Decision (2026-01-09)**: Yes. A singleton Create receives an **UnusedData warning**. This warning is **seal-insensitive** (always a warning, never an error). Rationale: Creating data that is never consumed is unusual and likely indicates a mistake, but it does not violate any hard constraint—the data lifecycle is technically complete (created, never accessed, implicitly discarded).

### Q2: What about Read+Read without Create?

Multiple Reads linked together, no Create. This is MissingCreate, same as other cases.

**Decision**: Already handled by the corrected MissingCreate condition. Any equivalence class with Read or Destroy but no Create triggers MissingCreate.

---

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-01-09 | Claude Opus 4.5 | Initial analysis; identified OrphanField conceptual error |
| 2026-01-09 | Claude Opus 4.5 | Resolved Q1: singleton Create receives UnusedData warning (seal-insensitive) |
| 2026-01-09 | Claude Opus 4.5 | Resolved Q2: Read+Read without Create is MissingCreate |
| 2026-01-09 | Claude Opus 4.5 | Updated diagnostic categories table with seal-sensitivity column |

---

*This document is Active and open for editing by both AI agents and humans.*
