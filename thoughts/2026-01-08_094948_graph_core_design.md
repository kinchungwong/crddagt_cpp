# 2026-01-08_094948_graph_core_design.md

- Author: Claude Opus 4.5 (Anthropic)
- Date: 2026-01-08 (America/Los_Angeles)
- Document status: Active

## Purpose

This document captures the design process for `graph_core.hpp` and related files, the central classes representing the CRDDAGT (Create, Read, Destroy DAG of Tasks) runtime structure. It includes reviews, identifies design questions, and tracks decisions as they are made.

## Terminology

### "Graph-Step-Field-Data" vs "Transaction/Task"

The internal naming convention **Graph-Step-Field-Data** is a legacy from a previous incarnation of this project. These terms were chosen for their specificity within the domain:

| Internal Term | General Audience Term | Description |
|---------------|----------------------|-------------|
| Graph | Task Graph | The overall structure representing execution dependencies |
| Step | Transaction / Task | A unit of execution that may access data |
| Field | Data Access Point | A step's typed reference to a piece of data |
| Data | Shared Resource | An abstract data object that flows through the graph |

The term **"transactions"** or **"tasks"** may be used in external documentation to make the concept more accessible to unfamiliar audiences. Similarly, **"Create-Read-Destroy"** (CRD) was chosen to relate to users familiar with the CRUD (Create, Read, Update, Delete) concept from database operations, though the semantics differ:

- **Create**: Produces/initializes the data (analogous to CRUD Create)
- **Read**: Consumes the data without modification (analogous to CRUD Read)
- **Destroy**: Finalizes/deallocates the data (analogous to CRUD Delete)

Note that there is no "Update" operation. A step that modifies data would have separate fields: one for reading the input and one for creating the output, which may reference the same or different data objects.

## File Structure

| File | Purpose |
|------|---------|
| `graph_core_enums.hpp` | `Usage` and `TrustLevel` enumerations |
| `graph_core_exceptions.hpp` | `GraphCoreError` exception class and `GraphCoreErrorCode` enum |
| `graph_core_diagnostics.hpp` | `GraphCoreDiagnostics` class (placeholder) |
| `exported_graph.hpp` | `ExportedGraph` struct - immutable snapshot of computed graph |
| `graph_core.hpp` | `GraphCore` class - mutable graph builder |

## Decisions Made

### Decision 1: Thread Safety Model

**Question**: What is the intended threading model for GraphCore?

**Decision**: **Externally synchronized**
- No internal synchronization
- Caller must synchronize all access
- Concurrent reads (const methods) are safe if no concurrent writes occur

**Applied to**: `GraphCore`, `ExportedGraph`, `GraphCoreError` documentation.

### Decision 2: Exception Type

**Question**: What exception type should be used for GraphCore errors?

**Decision**: **Custom exception with error code enum**
- New file `graph_core_exceptions.hpp`
- `GraphCoreError` class deriving directly from `std::exception`
- `GraphCoreErrorCode` enum (noted as subject to change)

**Error codes defined**:
- `InvalidStepIndex`, `InvalidFieldIndex`
- `DuplicateStepIndex`, `DuplicateFieldIndex`
- `TypeMismatch`, `UsageConstraintViolation`
- `CycleDetected`, `InvalidState`, `InvariantViolation`

### Decision 3: TrustLevel Semantics

**Question**: What does "blame" mean in the TrustLevel context?

**Decision**: **Blame for both cycles and constraint violations**
- When cycles are detected, lower-trust links are more likely to be identified as the problem
- When constraint violations occur, lower-trust links are suspected first
- `Low` = most likely to be blamed, `High` = least likely

### Decision 4: Ownership Model

**Clarification**: `GraphCore` uses **indices only** (Option B)
- GraphCore tracks relationships between indices
- Caller manages actual step/field objects externally (e.g., in `UniqueSharedWeakList`)
- GraphCore validates index sequentiality but not object existence

### Decision 5: Field Usage Semantics

**Clarification**: `Usage` is **per-field**, not per-link
- Each field has a fixed `Usage` (Create, Read, or Destroy) set at `add_field()` time
- `link_fields()` connects fields that reference the **same data**
- Execution order is automatically derived from the `Usage` values of linked fields

## Changes Applied

### 2026-01-08 (Session 2)

1. **Created `graph_core_exceptions.hpp`**
   - `GraphCoreErrorCode` enum with 9 error codes
   - `GraphCoreError` class with `code()` and `what()` methods

2. **Updated `exported_graph.hpp`**
   - Changed `class` to `struct` (all-public data)
   - Added comprehensive class-level documentation
   - Documented pair semantics: `(field_idx, data_object_idx)`, `(before_step_idx, after_step_idx)`
   - Added note that members are illustrative (subject to change)

3. **Updated `graph_core.hpp`**
   - Added class-level documentation with construction workflow
   - Added thread safety documentation
   - Added `step_count()` and `field_count()` query methods
   - Updated all `@throw` documentation to use `GraphCoreError`
   - Included `graph_core_exceptions.hpp`

4. **Updated `graph_core_enums.hpp`**
   - Enhanced `TrustLevel` documentation with blame priority and use cases

## Remaining Issues

### Issue 1: Private fields ~~still empty~~ RESOLVED

**Status**: Resolved

Private fields implemented with:
- Configuration: `m_eager_validation`
- Step tracking: `m_step_count`, `m_step_fields`
- Field tracking: `m_field_count`, `m_field_owner_step`, `m_field_types`, `m_field_usages`
- Explicit step links: `m_explicit_step_links`, `m_explicit_step_link_trust`
- Field links (union-find): `m_field_uf_parent`, `m_field_uf_rank`, `m_field_links`, `m_field_link_trust`
- Private helpers: `uf_find()`, `uf_unite()`

### Issue 2: GraphCoreDiagnostics ~~is placeholder~~ RESOLVED

**Status**: Resolved

Implemented with:
- `DiagnosticSeverity` enum: `Warning`, `Error`
- `DiagnosticCategory` enum: `Cycle`, `UsageConstraint`, `TypeMismatch`, `OrphanStep`, `OrphanField`, `InternalError`
- `DiagnosticItem` struct with severity, category, message, involved steps/fields, and blamed links
- `GraphCoreDiagnostics` class with:
  - `has_errors()`, `has_warnings()`, `is_valid()` query methods
  - `errors()`, `warnings()`, `all_items()` accessor methods
  - `friend class GraphCore` for population

### Issue 3: Implementation files ~~needed~~ RESOLVED

**Status**: Resolved

Created `graph_core.cpp` with:
- Constructor implementation
- `step_count()`, `field_count()` query methods
- `add_step()`, `add_field()` with validation
- `link_steps()`, `link_fields()` with validation
- Union-find `uf_find()`, `uf_unite()` helpers
- `get_diagnostics()` placeholder (TODO: full validation)
- `export_graph()` implementation with:
  - Field-to-data mapping via union-find
  - DataInfo population
  - Implicit step links from Usage ordering (Create < Read < Destroy)
  - Combined step links

## Open Design Questions

### Q1: Are Step and Field separate types or just indices?

**Decision**: **Type aliases to `size_t`**

The initial consideration was to use struct-based wrappers for compile-time type safety:
```cpp
struct StepIdx { size_t value; explicit StepIdx(size_t v); /* operators... */ };
```

This approach was rejected because:
- The type aliases exist for **clarity to human readers**, not compiler enforcement
- Struct wrappers would cause friction with upstream code expecting raw `size_t`
- The additional complexity does not justify the marginal benefit

Final implementation:
```cpp
using StepIdx = size_t;
using FieldIdx = size_t;
using DataIdx = size_t;
```

**Status**: Resolved

### Q2: Should `export_graph()` return `const`?

**Decision**: **Non-const return**

Keep `std::shared_ptr<ExportedGraph>` (non-const) to allow upstream code to move (take ownership of) the vectors rather than copying them. The data is conceptually immutable after construction, but the API should not prevent efficient transfer of ownership.

**Status**: Resolved

### Q3: Named structs vs pairs in ExportedGraph?

**Decision**: **Type aliases to `std::pair<size_t, size_t>`**

Use named type aliases that still alias to `std::pair` for interoperability with upstream code:
```cpp
using FieldDataPair = std::pair<size_t, size_t>;
using StepLinkPair = std::pair<size_t, size_t>;
```

This provides clarity in the API while maintaining compatibility with code that expects standard pairs.

**Status**: Resolved

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-01-08 | Claude Opus 4.5 | Initial review of graph_core.hpp draft |
| 2026-01-08 | Claude Opus 4.5 | Applied documentation updates, created exceptions, added query methods |
| 2026-01-08 | Claude Opus 4.5 | Resolved index type question: simple type aliases to size_t |
| 2026-01-08 | Claude Opus 4.5 | Added terminology section explaining Graph-Step-Field-Data legacy |
| 2026-01-08 | Claude Opus 4.5 | Added type aliases FieldDataPair/StepLinkPair in exported_graph.hpp |
| 2026-01-08 | Claude Opus 4.5 | Implemented GraphCore private fields with union-find for field equivalence |
| 2026-01-08 | Claude Opus 4.5 | Implemented GraphCoreDiagnostics with severity/category enums and blame analysis |
| 2026-01-08 | Claude Opus 4.5 | Created graph_core.cpp with full GraphCore implementation |

---

*This document is Active and open for editing by both AI agents and humans.*
