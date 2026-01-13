# 2026-01-12_203411_task_execution_roadmap.md

## Header

- **Title:** Task Execution Roadmap for crddagt_cpp
- **Author:** Claude (AI) with human direction
- **Date:** 2026-01-12 (America/Los_Angeles)
- **Document Status:** Active
- **Related:** `2026-01-12_215134_design_defects_and_pitfalls.md` (defects, pitfalls, questions)

---

## Executive Summary

This document captures the decision to add task execution capability to crddagt_cpp. Previously, task execution was explicitly out of scope to keep the project minimal. This pivot adds execution to the roadmap while:

1. Avoiding OpenCV dependency
2. Supporting pluggable thread execution backends
3. Maintaining the architectural simplicity that distinguishes crddagt_cpp from demo_002

---

## Precaution: Execution Phase Planning is Intentionally Vague

**The details in Phases 3-5 (execution backend) are intentionally vague and should NOT be regarded as authoritative.** Some aspects may be incorrect or require revision; these can only be resolved further down the road as implementation progresses.

### Intended Execution Model (High-Level Intent Only)

The actual execution backend is envisioned as:

- **Single task submission queue** of prerequisite-satisfied, ready-to-execute tasks
- **Variable number of workers** pulling from the queue
- **Tasks are assumed non-blocking** - not subject to potential deadlocking due to any sort of lock acquisitions within task code
- **CPU usage is intended to be very high**, possibly maxed out, with mechanisms to dial that back when needed

This differs from the "layered execution" model sketched in Phase 4-5 below. The layered model was an initial approximation; the queue-based model with pull-workers is the intended direction.

**Do not implement Phase 3-5 details literally.** Revisit and revise when reaching those phases.

---

## Background: Architectural Comparison with demo_002

Both projects implement task graph systems, but with fundamentally different designs. Understanding demo_002's complexity issues informs crddagt_cpp's execution design.

### demo_002 Complexity Issues (To Avoid)

| Issue | Description | Impact |
|-------|-------------|--------|
| **Monolithic seal()** | 1224-line method with 7 coupled stages | Cannot unit test individual stages |
| **Dual validation paths** | Legacy + new validation that don't align | Maintenance burden, inconsistent behavior |
| **Object-based core** | Heavy `shared_ptr` usage throughout | Unclear ownership, potential circular refs |
| **Lock-held pipeline** | Single mutex for entire graph during seal | No concurrent validation possible |
| **Testability crisis** | Can't test SortedUsageAnalysis standalone | Documented in 2025-12-24 design doc |

### crddagt_cpp Design Principles (To Maintain)

| Principle | Implementation | Benefit |
|-----------|----------------|---------|
| **Index-based core** | GraphCore tracks indices only | Clean separation, no smart pointers in core |
| **Append-only construction** | Steps/fields/links cannot be modified | Simple invariants, predictable behavior |
| **Modular validation** | Separate `get_diagnostics()` / `export_graph()` | Independent testing, clear responsibilities |
| **Token-based access** | CrdToken authorizes data operations | Explicit access control, debuggable |
| **Dual validation modes** | Eager or deferred via constructor flag | Same code path, different timing |

### Key Architectural Decision

**crddagt_cpp will NOT adopt demo_002's object-centric model.** Instead:

- `GraphCore` remains index-based and unchanged
- New `GraphBuilder` bridges indices to objects at the API boundary
- `ExecutableGraph` is a separate struct, not part of GraphCore
- Executors are independent, pluggable components

---

## Current State Assessment

### Working Components

| Component | File | Tests | Notes |
|-----------|------|-------|-------|
| GraphCore | graph_core.hpp/cpp | 254+ | Index-based DAG builder |
| ExportedGraph | exported_graph.hpp | Via GraphCore | Immutable snapshot |
| IterableUnionFind | iterable_union_find.hpp | Yes | Field equivalence classes |
| UniqueSharedWeakList | unique_shared_weak_list.hpp | Yes | Controlled ownership |

### Skeleton Components (Need Implementation)

| Component | File | Current State | Needed For |
|-----------|------|---------------|------------|
| VarData | vardata.hpp | Only `m_pvoid`, `m_ti` fields | Phase 1 |
| IStep | graph_items.hpp | Interface, no `execute()` | Phase 1 |
| IField | graph_items.hpp | Interface only | Phase 1 |
| IData | graph_items.hpp | Interface only | Phase 1 |
| GraphBuilder | graph_builder.hpp/cpp | Partial, TODO at line 36 | Phase 2 |

---

## Phased Implementation Plan

### Phase 1: Foundation

**Objective:** Complete VarData type erasure and solidify execution interfaces.

#### VarData Implementation

Current skeleton (vardata.hpp:19-27):
```cpp
class VarData {
    std::shared_ptr<void> m_pvoid{};
    std::type_index m_ti{typeid(void)};
};
```

Basic design:
- Type-erased storage class
- Type-aware users operate via type-parametrized methods (method templates).
- Use `shared_ptr<void>` with custom deleter that calls correct destructor.
- Store `type_index` for runtime type checking.

Intended use cases and semantics:
- Value-like.
- Safe for simultaneous reading and being copied from.
- No mutex within VarData itself.
- Owned by Data class (IData impl).
    - Data class manages access control, synchronization.

Invariants:
- `(m_ti == typeid(void))` iff `m_pvoid == nullptr`
- For all methods tparam `T` cannot be void, to avoid ambiguity with empty state.
- For all methods tparam `T` cannot be cv-qualified, array, etc. Enforced with `static_assert`. (List of forbidden types and qualifiers to be experimentally determined.)
- For all methods accepting user-provided `(type_index) ti`, `ti` cannot equal to `typeid(void)`.

Required additions:
- `has_value() const noexcept` - Check for empty state
- `bool has_type<T> const noexcept` - Check stored type is T
- `type() const noexcept` - Get stored type_index
- `reset() noexcept` - Clear to empty
- `emplace<T>(T&&)` - Factory (in inline header)
- `as<T>()`, `as<T>() const` - Type-safe access, `T&` or `const T&`, throws on empty or mismatch
- `get<T>()` - Returns `std::shared_ptr<T>` on match, nullptr otherwise
- `release<T>()` - Move out as `std::shared_ptr<T>`

#### IStep Enhancement

Add to graph_core_enums.hpp:
```cpp
enum class StepState
{
    NotReady,
    Ready,
    Queued,
    Executing,
    Succeeded,
    Failed,
    Cancelled
};
```

Add to graph_items.hpp:
```cpp
class IStep {
    // ... existing ...
    virtual void execute() = 0;  // NEW: Perform step's work
    virtual StepState state() const = 0;  // NEW: Current execution state
    virtual const std::string& class_name() const = 0;  // For type identification
    virtual std::string friendly_name() const = 0;  // For user display
    virtual std::string unique_name() const = 0;  // For unique id string
};
```

#### Reference Implementations

New files:
- `step_base.hpp` - CRTP base class for user steps
- `field_impl.hpp` - Concrete IField
- `data_impl.hpp` - Concrete IData with token validation

#### Phase 1 Risks

1. **VarData type erasure edge cases:** `const T` vs `T`, array types: disallowed, enforced.
2. **shared_from_this() UB:** Called before object fully constructed: avoid in constructors; docs needed.
3. **Token validation timing:** Lazy vs eager validation window: token only assigned immediately before graph transitions to execute.

---

### Phase 2: Graph-to-Execution Bridge

**Objective:** Connect validated graph structure to executable objects.

#### ExecutableGraph Structure

New file: `execution/executable_graph.hpp`

For illustration only; keep design malleable until implemented.

```cpp
struct ExecutableGraph {
    std::vector<StepPtr> steps;              // Actual step objects
    std::vector<DataPtr> data_objects;       // One per equivalence class
    std::vector<std::vector<StepIdx>> execution_layers;  // Topological layers
    std::vector<size_t> predecessor_counts;  // For ready-queue tracking
    std::vector<std::vector<StepIdx>> successors;  // For completion notification
    std::vector<CrdToken> step_tokens;       // Authorization tokens
    std::vector<std::vector<std::pair<StepIdx, Usage>>> data_access_rights;
};
```

#### GraphBuilder.build()

Complete `graph_builder.cpp` and add:
```cpp
std::shared_ptr<ExecutableGraph> GraphBuilder::build() {
    auto diagnostics = m_core->get_diagnostics(/*sealed=*/true);
    if (diagnostics->has_errors()) {
        throw std::runtime_error("Graph has validation errors");
    }

    auto exported = m_core->export_graph();
    auto exec = std::make_shared<ExecutableGraph>();

    // Populate from exported + m_steps + m_fields
    // Compute execution_layers via modified Kahn's algorithm
    // Assign tokens

    return exec;
}
```

#### Phase 2 Risks

1. **GraphBuilder TODO:** Line 36-38 must be fixed first
2. **Data lifetime:** Must ensure Data outlives all Step executions
3. **Layer computation:** Duplicate edges could corrupt predecessor counts

---

### Phase 3: Execution Backend Abstraction

**Objective:** Define pluggable executor interface.

#### IExecutor Interface

New file: `execution/executor_interface.hpp`

```cpp
class IExecutor {
public:
    virtual ~IExecutor() = default;
    virtual ExecutionResult execute(std::shared_ptr<ExecutableGraph> graph) = 0;
    virtual void request_stop() = 0;
    virtual bool stop_requested() const noexcept = 0;
};

struct ExecutionResult {
    bool success{true};
    std::vector<StepIdx> failed_steps;
    std::vector<std::string> error_messages;
    std::chrono::nanoseconds total_duration{0};
    std::vector<std::chrono::nanoseconds> step_durations;  // Optional
};
```

#### ExecutorConfig

```cpp
enum class ExecutorType { SingleThreaded, ThreadPool };

struct ExecutorConfig {
    ExecutorType type{ExecutorType::SingleThreaded};
    size_t thread_count{0};  // 0 = hardware_concurrency()
    bool collect_timing{false};
};
```

#### Phase 3 Decisions

- **Stop semantics:** Graceful; in-progress steps complete, pending steps skipped
- **Error handling:** First failure aborts (configurable in future)
- **Result ownership:** Returned by value, caller owns

---

### Phase 4: Reference Implementation

**Objective:** Single-threaded executor for testing and debugging.

#### SingleThreadedExecutor

```cpp
class SingleThreadedExecutor : public IExecutor {
public:
    ExecutionResult execute(std::shared_ptr<ExecutableGraph> graph) override {
        ExecutionResult result;
        for (const auto& layer : graph->execution_layers) {
            for (StepIdx sidx : layer) {
                if (m_stop_requested) { /* handle */ }
                try {
                    graph->steps[sidx]->execute();
                } catch (const std::exception& e) {
                    result.success = false;
                    result.failed_steps.push_back(sidx);
                    return result;  // Abort on first failure
                }
            }
        }
        return result;
    }
};
```

#### Test Cases

- Empty graph
- Linear chain (A -> B -> C)
- Diamond pattern (A -> B, A -> C, B -> D, C -> D)
- Stop request handling
- Exception propagation

---

### Phase 5: Parallel Execution (Optional)

**Objective:** Thread pool executor for production use.

#### ThreadPoolExecutor Design

- Bounded thread pool (configurable size)
- Work-stealing queue (or simple mutex-protected queue initially)
- Atomic predecessor counts for ready-queue tracking
- Condition variable for completion waiting

#### Critical Requirement

IData implementations must support concurrent Read access:
- Create: Exclusive (no concurrent access)
- Read: Shared (multiple concurrent reads OK)
- Destroy: Exclusive (no concurrent access)

This matches the CRD semantics already defined.

---

## Design Defects and Open Questions

**See:** `2026-01-12_215134_design_defects_and_pitfalls.md`

The defects, pitfalls, architectural dangers, and open questions have been moved to a separate document to keep this roadmap focused on implementation phases.

Key categories covered in that document:
- 6 high-confidence defects (code issues requiring fixes)
- 6 medium-confidence pitfalls (potential problems)
- 5 architectural dangers (execution-phase risks)
- 13 open questions organized by phase
- Lessons learned from demo_002

---

## File Structure After Full Implementation

```
crddagt_cpp/src/crddagt/
├── common/
│   ├── graph_core.hpp              # Unchanged
│   ├── graph_core.cpp              # Unchanged
│   ├── exported_graph.hpp          # Unchanged
│   ├── graph_items.hpp             # Modified: IStep.execute()
│   ├── graph_builder.hpp           # Modified: build()
│   ├── graph_builder.cpp           # Modified: complete + build()
│   ├── vardata.hpp                 # Modified: complete
│   ├── vardata.inline.hpp          # Modified: templates
│   ├── step_base.hpp               # NEW
│   ├── field_impl.hpp              # NEW
│   ├── data_store.hpp              # NEW
│   └── data_store.cpp              # NEW
└── execution/                       # NEW directory
    ├── executable_graph.hpp
    ├── executor_interface.hpp
    ├── execution_result.hpp
    ├── executor_factory.hpp
    ├── executor_factory.cpp
    ├── single_threaded_executor.hpp
    ├── single_threaded_executor.cpp
    ├── thread_pool_executor.hpp     # Phase 5
    └── thread_pool_executor.cpp     # Phase 5

crddagt_cpp/tests/
├── common/
│   ├── vardata_tests.cpp           # NEW
│   ├── step_base_tests.cpp         # NEW
│   └── data_store_tests.cpp        # NEW
└── execution/                       # NEW directory
    ├── executable_graph_tests.cpp
    ├── single_threaded_executor_tests.cpp
    └── thread_pool_executor_tests.cpp  # Phase 5
```

---

## Next Steps

1. **Phase 1 implementation** - Start with VarData, most foundational
2. **Fix GraphBuilder TODO** - Required before Phase 2
3. **Document IData thread safety** - Add to interface before implementations
4. **Create test harness** - Especially for VarData type erasure edge cases

---

## References

- `graph_core.hpp` - Existing index-based design
- `exported_graph.hpp` - Output structure to build upon
- `demo_002/tg/graph.cpp` - Anti-pattern reference (what to avoid)
- `thoughts/2026-01-10_071521_dangers_of_invalid_graph_instantiation.md` - Safety considerations
