# 2026-01-12_215134_design_defects_and_pitfalls.md

## Header

- **Title:** Design Defects, Pitfalls, and Open Questions
- **Author:** Claude (AI) with human direction
- **Date:** 2026-01-12 (America/Los_Angeles)
- **Document Status:** Active
- **Related:** `2026-01-12_203411_task_execution_roadmap.md` (implementation phases)

---

## Purpose

This document catalogs known defects, potential pitfalls, architectural dangers, and open questions for the crddagt_cpp task execution implementation. It is separated from the roadmap to keep concerns distinct.

**Confidence levels:**
- **HIGH** - Known issues requiring fixes before proceeding
- **MEDIUM** - Potential problems depending on usage patterns
- **LOW** - Theoretical risks; monitor but don't block on

---

## 1. HIGH CONFIDENCE DEFECTS

These are known code issues that must be resolved.

### D1: GraphBuilder.add_field() TODO Incomplete
- **Location:** `src/crddagt/common/graph_builder.cpp:35-38`
- **Confidence:** HIGH
- **Code:**
  ```cpp
  if (has_added_step) {
      /// @todo
  }
  ```
- **Problem:** When a field's owning step hasn't been added to GraphBuilder yet, the step registration is incomplete. This causes index misalignment between `m_steps` and `GraphCore`'s internal step count.
- **Impact:** Must be fixed before Phase 2 (GraphBuilder.build())
- **Resolution:** Add the step to GraphCore when encountered via field

### D2: VarData Stub Implementation
- **Location:** `src/crddagt/common/vardata.hpp:13-27`
- **Confidence:** HIGH
- **Problem:** Only skeleton with `m_pvoid` and `m_ti` fields; no methods implemented
- **Missing:**
  - `has_value()`, `type()`, `reset()`
  - `make<T>()`, `as<T>()`, `try_as<T>()`, `take<T>()`
- **Impact:** Phase 1 blocker - cannot store or retrieve typed data
- **Resolution:** Implement in vardata.hpp and vardata.inline.hpp

### D3: IData Thread-Safety Unspecified
- **Location:** `src/crddagt/common/graph_items.hpp:50-64`
- **Confidence:** HIGH
- **Problem:** Interface doesn't document concurrent access semantics
- **Required semantics for parallel execution:**
  - Create: Exclusive (no concurrent access)
  - Read: Shared (multiple concurrent reads OK)
  - Destroy: Exclusive (no concurrent access)
- **Impact:** Phase 5 blocker without specification
- **Resolution:** Add Doxygen documentation specifying requirements; implementations use `std::shared_mutex` or similar

### D4: CrdToken No Validation Mechanism
- **Location:** `src/crddagt/common/graph_items.hpp:17`
- **Confidence:** HIGH
- **Problem:** `using CrdToken = size_t;` with no authorization mechanism
- **Impact:** IData implementations cannot enforce access control; any token value is accepted
- **Resolution:** Design token issuance in GraphBuilder.build(); store token-to-rights mapping in ExecutableGraph; IData validates against rights

### D5: IStep.execute() Missing
- **Location:** `src/crddagt/common/graph_items.hpp:19-31`
- **Confidence:** HIGH
- **Problem:** Interface has no method for task execution
- **Impact:** Phase 1 blocker - steps cannot be executed
- **Resolution:** Add `virtual void execute() = 0;`

### D6: GraphBuilder.build() Missing
- **Location:** `src/crddagt/common/graph_builder.hpp`
- **Confidence:** HIGH
- **Problem:** No method to finalize graph and produce ExecutableGraph
- **Impact:** Phase 2 blocker
- **Resolution:** Implement build() that calls get_diagnostics(), export_graph(), and constructs ExecutableGraph

---

## 2. MEDIUM CONFIDENCE PITFALLS

These are potential problems depending on usage patterns.

### P1: OpaquePtrKey Address Reuse Risk
- **Location:** `src/crddagt/common/opaque_ptr_key.hpp:36-43`
- **Confidence:** MEDIUM
- **Problem:** Uses `reinterpret_cast<std::uintptr_t>(ptr.get())` to capture address. If object is destroyed and new object allocated at same address, keys collide.
- **Documentation:** Already documented in header as known limitation
- **Mitigation:** Callers must keep objects alive via `shared_ptr` while keys are in use
- **Risk Level:** Low if usage guidelines followed; high if violated

### P2: WeakPtr Expiration in UniqueSharedWeakList
- **Location:** `src/crddagt/common/unique_shared_weak_list.hpp`
- **Confidence:** MEDIUM
- **Problem:** List stores weak pointers; `expired_entry_error` thrown if accessed after expiration
- **Context:** GraphBuilder uses `UniqueSharedWeakList<IStep>` and `UniqueSharedWeakList<IField>`
- **Risk:** If external code deletes step/field, list operations will throw
- **Mitigation:** Document that GraphBuilder expects callers to maintain object lifetimes

### P3: shared_from_this() Undefined Behavior
- **Location:** `src/crddagt/common/graph_items.hpp:23, 37, 54`
- **Confidence:** MEDIUM
- **Problem:** Interfaces require `shared_from_this()` but don't mandate `std::enable_shared_from_this<T>` inheritance
- **Risks:**
  1. Implementation doesn't inherit from enable_shared_from_this -> UB
  2. Called during construction before managed by shared_ptr -> throws `std::bad_weak_ptr`
- **Mitigation:** Document requirement; provide StepBase CRTP class that handles this correctly

### P4: Type Mismatch Edge Cases with type_index
- **Location:** `src/crddagt/common/graph_core.cpp:339-340`
- **Confidence:** MEDIUM
- **Problem:** `std::type_index` comparison treats these as different:
  - `const T` vs `T`
  - `T&` vs `T`
  - `T[N]` vs `T*`
- **Impact:** Two fields for "same" data with subtly different types rejected as TypeMismatch
- **Resolution Options:**
  1. Document as intentional (strict typing)
  2. Strip cv-qualifiers in VarData
  3. Provide type normalization utilities

### P5: MissingCreate Seal-Sensitivity
- **Location:** `src/crddagt/common/graph_core.cpp`, `thoughts/2026-01-09_153426_missing_create_seal_sensitivity.md`
- **Confidence:** MEDIUM
- **Problem:** MissingCreate severity should vary:
  - During construction: Warning (Create step may be added later)
  - After sealing: Error (graph is complete, Create is truly missing)
- **Current State:** `get_diagnostics(bool treat_as_sealed)` parameter exists; verify implementation is correct
- **Action:** Review and test seal-sensitivity behavior

### P6: Index Sequencing Requirement
- **Location:** `src/crddagt/common/graph_core.hpp:75, 84`
- **Confidence:** MEDIUM
- **Problem:** Steps and fields must be added with sequential indices (0, 1, 2, ...) with no gaps
- **Code:**
  ```cpp
  if (step_idx != m_step_count) { /* error */ }
  ```
- **Risk:** Not well-documented; callers may not realize this constraint
- **Mitigation:** Document prominently; GraphBuilder handles sequencing internally

---

## 3. LOW CONFIDENCE EDGE CASES

These are theoretical risks to monitor.

### E1: Const Pointer Casting in UniqueSharedWeakList
- **Location:** `src/crddagt/common/unique_shared_weak_list.hpp:128, 166`
- **Confidence:** LOW
- **Problem:** Uses `std::const_pointer_cast<T>` internally
- **Risk:** Type safety erosion; assumes no actual const semantics
- **Assessment:** Deliberate design choice; fragile but functional

### E2: Modification During Enumeration
- **Location:** `src/crddagt/common/opk_unique_list.hpp:141`, `unique_shared_weak_list.hpp:389`
- **Confidence:** LOW
- **Problem:** Documentation warns: "Do not modify the list from inside the callback; behavior is undefined"
- **Risk:** No runtime enforcement; if violated, iterator invalidation
- **Mitigation:** Documented; caller responsibility

### E3: reinterpret_cast to uintptr_t
- **Location:** `src/crddagt/common/opaque_ptr_key.hpp:62-92`
- **Confidence:** LOW
- **Problem:** Technically implementation-defined in C++ standard
- **Assessment:** Universally supported on target platforms (x86_64 Linux); safe in practice

---

## 4. ARCHITECTURAL DANGERS (Execution Phase)

From `thoughts/2026-01-10_071521_dangers_of_invalid_graph_instantiation.md`:

These risks materialize if an invalid graph is executed despite validation.

### A1: Circular Ownership Memory Leak
- **Trigger:** Cyclic graph instantiated with tasks holding `shared_ptr` to dependencies
- **Consequence:** Reference counts never reach zero; permanent memory leak
- **Mitigation:** Use weak references for back-edges; executor owns tasks and breaks cycles

### A2: Infinite Wait / Deadlock
- **Trigger:** Cyclic graph executed
- **Consequence:** Tasks in cycle never become ready; Kahn's algorithm never starts them; any wait on completion blocks indefinitely
- **Mitigation:** Validate graph before execution; executor should detect and report stalled tasks with timeout

### A3: Access to Nonexistent Data (MissingCreate)
- **Trigger:** MissingCreate error exists but graph is executed
- **Consequence:** Task tries to read/destroy data that was never created
- **Severity ranges:**
  - Static allocation: Undefined behavior
  - Raw pointers: Crash (nullptr dereference)
  - VarData without protection: Exception
- **Mitigation:** Executor must refuse graphs with MissingCreate; checked access layer prevents UB

### A4: Double Destroy
- **Trigger:** MultipleDestroy error - two tasks destroy same data
- **Consequence:** First destroy deallocates; second operates on invalid memory; potential heap corruption
- **Mitigation:** Executor refuses MultipleDestroy graphs; checked access layer enforces single-destroy

### A5: Data Race from Multiple Create
- **Trigger:** MultipleCreate error - two tasks create same data concurrently
- **Consequence:** Data race, corrupted initial state, resource leaks
- **Mitigation:** Executor refuses MultipleCreate graphs; checked access layer enforces single-create

---

## 5. LESSONS FROM demo_002

Analysis of demo_002's commit history and design documents reveals patterns to avoid and adopt.

### 5.1 Critical Mistakes to Avoid

#### M1: Monolithic seal() Pipeline
- **Evidence:** `demo_002/src/demo_002/tg/graph.cpp` - 1224 lines in seal()
- **Problem:** All validation, construction, and state transitions in single locked function
- **Impact:** Cannot unit test individual stages; failures are ambiguous
- **Lesson:** Separate concerns from day one; explicit phases with single responsibilities

#### M2: Dual Validation Paths
- **Evidence:** Legacy `lck_init_data_infos()` + new `SortedUsageAnalysis` paths coexist
- **Problem:** Neither path is authoritative; new path has false negatives
- **Impact:** Maintenance burden; inconsistent behavior
- **Lesson:** When migrating validation, have strict equivalence tests; never deprecate until proven

#### M3: Testability Crisis
- **Evidence:** `demo_002/thoughts/2025-12-24_135729_graph_seal_testability_review.md`
- **Problem:** No standalone constructors for analysis components; diagnostics are debug-only (cout)
- **Impact:** Cannot unit test validation logic; must run full seal() to test anything
- **Lesson:** Design diagnostics as first-class queryable objects with stable APIs from start

#### M4: State Accumulation Without Reset
- **Evidence:** `demo_002/thoughts/2025-12-10_213127_field-deps-refactor-analysis.md`
- **Problem:** `m_data_infos` and `m_step_links_implicit` accumulated across calls; no reset semantics
- **Impact:** Asserts fired unpredictably; state corruption
- **Lesson:** Require explicit reset semantics; make idempotence testable

#### M5: Overgeneralized Solutions
- **Evidence:** Commit `adbf11f` - "wrong fnv1a high level design, currently broken"
- **Problem:** Attempted universal aggregate hashing; 300+ lines for wrong problem
- **Impact:** Scope creep; wasted effort; broken code
- **Lesson:** Identify minimal problem; fix only that; resist "solve everything" urge

#### M6: Code Duplication
- **Evidence:** Commit `9df4325` - "sorted_usage_analysis.cpp contained duplicated source code"
- **Problem:** Analysis code copy-pasted; bugs in one copy not fixed everywhere
- **Lesson:** Extract reusable logic first; code review for duplication

### 5.2 Patterns to Adopt

1. **Separate concerns into explicit phases** with single responsibilities
2. **Make every component independently testable** with pure-input constructors
3. **Define validation rules as specification first** before implementation
4. **Create diagnostics as first-class queryable objects** (not cout)
5. **Enforce precondition chains** between phases
6. **Validate combined graphs**, not just components (implicit + explicit deps)
7. **Make reset semantics explicit** and testable
8. **Set strict compiler flags** (`-Werror=return-type`) from day one

### 5.3 Key Quote

> "Good engineering is not about solving everything. It is about solving the right thing, at the right layer, with the least irreversible commitment."
>
> — From `demo_002/thoughts/2025-12-13_215200_ai_response_to_original_goal_of_custom_hashing.md`

---

## 6. OPEN QUESTIONS TO RESOLVE

Questions are organized by implementation phase for easier triage.

### 6.1 Phase 1 Questions (Foundation)

**Q1: VarData - Should `as<T>()` throw or return optional?**
- **Trade-off:** Exceptions vs explicit error handling
- **Options:**
  - `as<T>()` throws `std::bad_cast` on mismatch
  - `try_as<T>()` returns `T*` (nullptr on mismatch) - already planned
  - `as_opt<T>()` returns `std::optional<T&>` - C++17 doesn't have optional<T&>
- **Recommendation:** Keep throw semantics for `as<T>()`; provide `try_as<T>()` for non-throwing path

**Q2: VarData - How to handle `const T` vs `T`?**
- **Options:**
  - Strip const: `std::decay_t<T>` for storage; allow const access
  - Reject: Different type_index = different type, by design
  - Document: Just document as limitation
- **Recommendation:** Use `std::decay_t<T>` for storage type; document behavior

**Q3: IStep - Should execute() return a result or throw on failure?**
- **Trade-off:** Affects executor error handling design
- **Options:**
  - `void execute()` throws on failure
  - `bool execute()` returns false on failure
  - `ExecutionResult execute()` returns structured result
- **Recommendation:** `void execute()` with exceptions; executor catches and records

**Q4: IData - What synchronization primitive for concurrent Reads?**
- **Options:**
  - `std::shared_mutex` - standard, heavier
  - External synchronization by executor - more flexible, less encapsulated
  - No synchronization in default impl; executor guarantees non-concurrent access
- **Recommendation:** Document requirement; let implementations choose; provide thread-safe default

### 6.2 Phase 2 Questions (Bridge)

**Q5: Token issuance - Who assigns CrdTokens?**
- **Options:**
  - `GraphBuilder.build()` assigns during construction
  - Executor assigns at runtime
  - External token service
- **Recommendation:** `GraphBuilder.build()` assigns; stored in ExecutableGraph

**Q6: Token validation - Where enforced?**
- **Options:**
  - IData implementation validates
  - Executor wrapper validates before calling IData
  - Both (defense in depth)
- **Recommendation:** IData validates; executor is unaware of token internals

**Q7: Data lifetime - Who owns DataPtr after build()?**
- **Options:**
  - ExecutableGraph owns exclusively
  - Shared between ExecutableGraph and GraphBuilder
  - External ownership; ExecutableGraph holds weak refs
- **Recommendation:** ExecutableGraph owns; GraphBuilder releases after build()

### 6.3 Phase 3-5 Questions (Execution - Intentionally Vague)

**Note:** These questions are captured for future consideration but should NOT be resolved now. The execution model is still being refined.

**Q8: Execution model - Ready-queue vs layer-based?**
- **Current direction:** Single ready-queue with pull-workers (per user)
- **Action:** Revisit when reaching Phase 3

**Q9: Worker count - Fixed or dynamic?**
- **Considerations:** "Dial back" mechanism mentioned by user
- **Action:** Defer design until Phase 5

**Q10: Task blocking - What if task violates non-blocking assumption?**
- **Considerations:** User stated "tasks are assumed non-blocking"
- **Action:** Document as caller responsibility; consider timeout detection

### 6.4 Architectural Questions

**Q11: Error recovery - Abort-first vs continue-parallel-paths?**
- **Trade-off:** Different use cases
- **Options:**
  - Abort on first failure (simpler, deterministic)
  - Continue independent paths (maximizes useful work)
  - Configurable per-execution
- **Recommendation:** Start with abort-first; add configurable behavior later

**Q12: Diagnostics during execution - Real-time progress?**
- **Considerations:** Timing, profiling, progress callbacks
- **Action:** Phase 5 consideration; not needed for reference implementation

**Q13: Graph reuse - Can same ExecutableGraph be executed multiple times?**
- **Considerations:** Data must be reset between executions
- **Options:**
  - Single-use ExecutableGraph
  - Reusable with explicit reset() method
  - Clone for each execution
- **Recommendation:** Start single-use; add reset() if needed

---

## 7. References

- `2026-01-12_203411_task_execution_roadmap.md` - Implementation phases
- `2026-01-10_071521_dangers_of_invalid_graph_instantiation.md` - Architectural dangers
- `2026-01-09_153426_missing_create_seal_sensitivity.md` - Seal-sensitivity analysis
- `demo_002/thoughts/2025-12-24_135729_graph_seal_testability_review.md` - Testability lessons
