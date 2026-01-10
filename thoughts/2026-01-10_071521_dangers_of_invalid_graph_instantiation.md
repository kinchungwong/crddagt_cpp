# 2026-01-10_071521_dangers_of_invalid_graph_instantiation.md

- Author: Claude Opus 4.5 (Anthropic)
- Date: 2026-01-10 (America/Los_Angeles)
- Document status: Active
- Document type: Reference

## See Also

- [2026-01-10_065043_diagnostic_revamp_mikado_plan.md](2026-01-10_065043_diagnostic_revamp_mikado_plan.md) - Mikado plan that references this document
- [2026-01-09_155955_orphan_field_conceptual_error.md](2026-01-09_155955_orphan_field_conceptual_error.md) - Defect analysis for diagnostic categories

---

## Dangers of Invalid Graph Instantiation

When an invalid graph is exported and instantiated (e.g., task objects created, ownership established), several dangerous conditions can arise. These must be documented and understood by users of the forced export feature.

### Assumptions

- Since this project is intended to be used as a building block for another system, some of the dangers are mitigated, though not all of them can be. The first two (circular ownership memory leak and infinite wait) are always relevant.
- An important mitigation is the presence of a thread-safe and checked data access layer. Because all accesses are checked, specific classes of undefined behaviors and memory corruptions are prevented, assuming that all task field usages are declared correctly.
- Without a thread-safe and checked data access layer, all of the documented dangers are relevant.

### Danger 1: Circular Ownership Memory Leak

**Scenario**: Graph contains a cycle (detected but exported anyway via forced export).

**Mechanism**: If task objects hold strong references (e.g., `std::shared_ptr`) to their dependencies, and those dependencies transitively reference back, a reference cycle forms.

**Consequence**: Reference counts never reach zero. Memory is never freed. This is a **memory leak** that persists for the lifetime of the process.

**Mitigation**:
- Use weak references (`std::weak_ptr`) for back-edges or non-owning references
- Ensure the executor owns all tasks and breaks cycles on shutdown
- Document that cyclic graphs must not use naive shared_ptr ownership

### Danger 2: Unsatisfiable Tasks and Infinite Wait

**Scenario**: Graph has a cycle, or MissingCreate leaves a data dependency unsatisfied.

**Mechanism**: Kahn's algorithm (or similar topological executor) starts tasks only when all dependencies are satisfied. In a cycle, no task in the cycle has all dependencies satisfied—each waits for another.

**Consequence**: Tasks in the cycle **never start**. Any code that waits for graph completion (e.g., `future.wait()`, `condition_variable.wait()`) will block **indefinitely**.

**Mitigation**:
- Executor should detect and report stalled tasks
- Implement timeout mechanisms for graph completion waits
- Validate graph before execution; forced export should only be used for inspection, not execution

### Danger 3: Access to Nonexistent Data

**Scenario**: MissingCreate error—a Read or Destroy field references data that has no Create.

**Mechanism**: The task attempts to access data that was never created. The outcome depends on the data management strategy:

| Implementation | Consequence |
|----------------|-------------|
| Static allocation | Access to uninitialized memory; undefined behavior |
| Raw pointers | Null or dangling pointer dereference; crash or corruption |
| Unprotected VarData | Empty or null shared_ptr; exception or undefined behavior on dereference |
| Checked access layer | Access denied; error reported; task fails gracefully |

**Mitigation**:
- Executor must refuse to run graphs with MissingCreate errors
- Forced export should embed MissingCreate as an error marker on affected records
- Checked access layer prevents undefined behavior by validating data existence

### Danger 4: Double Destroy

**Scenario**: MultipleDestroy error—two tasks attempt to destroy the same data.

**Mechanism**: The first destroy deallocates or invalidates the data; the second destroy operates on invalid state. The outcome depends on the data management strategy:

| Implementation | Consequence |
|----------------|-------------|
| Static allocation | Double-reset or no-op (may be benign or corrupt state) |
| Raw pointers | Double-free; heap corruption, crash, or security exploit |
| Unprotected VarData | Double shared_ptr reset; typically benign (refcount already zero) but semantically wrong |
| Checked access layer | Second destroy denied; error reported; no corruption |

**Mitigation**:
- Executor must refuse to run graphs with MultipleDestroy errors
- Ownership model must ensure exactly one destroyer
- Checked access layer enforces single-destroy semantics

### Danger 5: Data Race from Multiple Create

**Scenario**: MultipleCreate error—two tasks attempt to create the same data concurrently.

**Mechanism**: Both tasks attempt to initialize the same data slot without synchronization. The outcome depends on the data management strategy:

| Implementation | Consequence |
|----------------|-------------|
| Static allocation | Data race; torn writes; corrupted initial state |
| Raw pointers | Memory leak (one allocation lost) or double-init of same memory |
| Unprotected VarData | Last-write-wins race; one shared_ptr silently replaced; resource leak |
| Checked access layer | Second create denied; error reported; first create preserved |

**Mitigation**:
- Executor must refuse to run graphs with MultipleCreate errors
- Checked access layer enforces single-create semantics with atomic registration

---

## Summary Table

| Danger | Diagnostic | Always Relevant | Mitigated by Checked Layer |
|--------|------------|-----------------|---------------------------|
| Circular Ownership Memory Leak | Cycle | Yes | No |
| Unsatisfiable Tasks / Infinite Wait | Cycle, MissingCreate | Yes | No |
| Access to Nonexistent Data | MissingCreate | No* | Yes |
| Double Destroy | MultipleDestroy | No* | Yes |
| Data Race from Multiple Create | MultipleCreate | No* | Yes |

\* These dangers manifest as undefined behavior only without a checked access layer. With a checked layer, they become graceful failures.

---

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-01-10 | Claude Opus 4.5 | Initial document extracted from Mikado plan |
| 2026-01-10 | User | Added Assumptions subsection |
| 2026-01-10 | Claude Opus 4.5 | Revised Dangers 3-5 with implementation-specific consequence tables |
| 2026-01-10 | Claude Opus 4.5 | Added summary table |

---

*This document is Active and open for editing by both AI agents and humans.*
