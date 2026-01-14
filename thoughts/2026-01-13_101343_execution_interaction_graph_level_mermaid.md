# 2026-01-13_101343_execution_interaction_graph_level_mermaid.md

## Header

- **Title:** Execution Interaction Diagrams: Graph Level (Mermaid)
- **Author:** Claude (AI) with human direction
- **Date:** 2026-01-13 (America/Los_Angeles)
- **Document Status:** Active
- **Related:**
  - `2026-01-13_095409_execution_interaction_graph_level.md` (ASCII version)
  - `2026-01-13_095410_execution_interaction_step_level.md`
  - `2026-01-13_095411_execution_interaction_data_flow.md`
  - `2026-01-12_203411_task_execution_roadmap.md`

---

## Purpose

This document illustrates the graph-level execution lifecycle through chronological interaction diagrams using **Mermaid**. It is a companion to the ASCII version and covers the same scenarios.

**Note:** These diagrams are design exploration, not authoritative specifications. The actual execution model uses a single task submission queue with variable workers pulling prerequisite-satisfied tasks.

---

## Actors

| Actor | Responsibility |
|-------|----------------|
| **User** | Application code that defines and executes graphs |
| **GraphBuilder** | Bridges user objects to index-based GraphCore |
| **GraphCore** | Index-based DAG validation and export |
| **ExecutableGraph** | Immutable execution plan with objects and ordering |
| **Executor** | Manages execution lifecycle and worker coordination |
| **ReadyQueue** | Thread-safe queue of prerequisite-satisfied tasks |
| **Worker** | Thread that pulls and executes tasks |

---

## Scenario 1: Successful Execution (Happy Path)

A graph with steps A->B->C executes successfully.

```mermaid
sequenceDiagram
    participant User
    participant GraphBuilder
    participant GraphCore
    participant ExecGraph as ExecutableGraph
    participant Executor
    participant ReadyQueue
    participant Worker

    rect rgb(240, 248, 255)
        Note over User,GraphCore: CONSTRUCTION PHASE
        User->>GraphBuilder: add_step(A)
        GraphBuilder->>GraphCore: add_step(0)
        GraphBuilder-->>User: ok

        User->>GraphBuilder: add_step(B)
        GraphBuilder->>GraphCore: add_step(1)
        GraphBuilder-->>User: ok

        User->>GraphBuilder: link(A,B)
        GraphBuilder->>GraphCore: link(0,1)
        GraphBuilder-->>User: ok

        Note over User,GraphCore: ... (add C, link B->C) ...
    end

    rect rgb(255, 250, 240)
        Note over User,ExecGraph: BUILD PHASE
        User->>GraphBuilder: build()
        GraphBuilder->>GraphCore: diagnostics(sealed=true)
        GraphCore-->>GraphBuilder: DiagResult(no errors)
        GraphBuilder->>GraphCore: export_graph()
        GraphCore-->>GraphBuilder: ExportedGraph
        GraphBuilder->>ExecGraph: create ExecutableGraph
        Note over GraphBuilder,ExecGraph: assign tokens, compute layers
        GraphBuilder-->>User: ExecGraph
    end

    rect rgb(240, 255, 240)
        Note over User,Worker: EXECUTION PHASE
        User->>Executor: execute(graph)
        Executor->>ReadyQueue: populate
        Note over Executor: A has no preds
        Executor->>ReadyQueue: enqueue(A)

        Worker->>ReadyQueue: pull
        ReadyQueue-->>Worker: A
        Note over Worker: Worker executes A
        Worker-->>ReadyQueue: done(A)
        ReadyQueue->>Executor: notify(A complete)

        Note over Executor: B's preds satisfied
        Executor->>ReadyQueue: enqueue(B)
        Worker->>ReadyQueue: pull
        ReadyQueue-->>Worker: B
        Note over Worker: Worker executes B
        Worker-->>ReadyQueue: done(B)
        ReadyQueue->>Executor: notify(B complete)

        Note over Executor: C's preds satisfied
        Executor->>ReadyQueue: enqueue(C)
        Worker->>ReadyQueue: pull
        ReadyQueue-->>Worker: C
        Note over Worker: Worker executes C
        Worker-->>ReadyQueue: done(C)
        ReadyQueue->>Executor: notify(C complete)

        Note over Executor: all steps complete
        Executor-->>User: result(success)
    end
```

---

## Scenario 2: Validation Failure at build()

Graph has a cycle (A->B->A), detected during build().

```mermaid
sequenceDiagram
    participant User
    participant GraphBuilder
    participant GraphCore

    User->>GraphBuilder: add_step(A)
    GraphBuilder->>GraphCore: add_step(0)
    User->>GraphBuilder: add_step(B)
    GraphBuilder->>GraphCore: add_step(1)
    User->>GraphBuilder: link(A,B)
    GraphBuilder->>GraphCore: link(0,1)
    User->>GraphBuilder: link(B,A)
    GraphBuilder->>GraphCore: link(1,0)
    Note over GraphCore: creates cycle

    User->>GraphBuilder: build()
    GraphBuilder->>GraphCore: diagnostics(sealed=true)
    GraphCore-->>GraphBuilder: DiagResult(Cycle error)

    Note over GraphBuilder: check has_errors() = true
    Note over GraphBuilder: cycle detected

    GraphBuilder--xUser: THROW GraphValidationError
    Note over User: "Cycle detected: steps 0,1"
    Note over User: no ExecutableGraph created
    Note over User: no execution occurs
```

**Notes:**
- Validation happens eagerly at build() time
- User receives structured error with involved steps
- No resources allocated for execution

---

## Scenario 3: Step Failure During Execution

Step B throws an exception during execute().

```mermaid
sequenceDiagram
    participant User
    participant Executor
    participant ReadyQueue
    participant Worker
    participant StepA
    participant StepB
    participant StepC

    User->>Executor: execute()
    Executor->>ReadyQueue: enqueue(A)
    Worker->>ReadyQueue: pull
    ReadyQueue-->>Worker: A
    Worker->>StepA: exec
    StepA-->>Worker: ok
    Worker->>Executor: done(A)

    Executor->>ReadyQueue: enqueue(B)
    Worker->>ReadyQueue: pull
    ReadyQueue-->>Worker: B
    Worker->>StepB: exec
    StepB--xWorker: THROW std::runtime_error

    Worker->>Executor: failed(B, error)

    Note over Executor: abort execution
    Note over Executor: mark C as Cancelled
    Note over Executor: collect results
    Note over Executor: success=false, failed_steps=[1]

    Executor-->>User: result
    Note over User: success=false
    Note over User: failed_steps=[B]
```

**Notes:**
- First failure triggers abort (configurable behavior in future)
- Pending steps (C) are marked Cancelled, not executed
- Worker catches exception, reports to Executor
- Result contains which step failed and error message

---

## Scenario 4: Stop Request (Graceful Shutdown)

User requests stop while execution is in progress.

```mermaid
sequenceDiagram
    participant User
    participant Executor
    participant ReadyQueue
    participant Worker1
    participant Worker2
    participant StepA
    participant StepB
    participant StepC

    User->>Executor: execute()
    Executor->>ReadyQueue: enqueue(A)
    Executor->>ReadyQueue: enqueue(B)
    Note over ReadyQueue: both ready, no deps

    Worker1->>ReadyQueue: pull
    ReadyQueue-->>Worker1: A
    Worker2->>ReadyQueue: pull
    ReadyQueue-->>Worker2: B

    par Concurrent Execution
        Worker1->>StepA: exec
    and
        Worker2->>StepB: exec
    end

    User->>Executor: request_stop()
    Note over Executor: set stop flag

    StepA-->>Worker1: ok
    Worker1->>Executor: done(A)

    StepB-->>Worker2: ok
    Worker2->>Executor: done(B)

    Note over Executor: check stop flag: true
    Note over Executor: skip enqueuing C
    Note over Executor: mark C as Cancelled

    Executor-->>User: result
    Note over User: success=false
    Note over User: error="stopped by request"
    Note over User: completed=[A,B]
    Note over User: cancelled=[C]
```

**Notes:**
- In-progress steps (A, B) complete normally
- Pending steps (C) are not started, marked Cancelled
- Stop is cooperative, not preemptive
- Result indicates partial completion

---

## Scenario 5: Empty Graph Execution

Graph has no steps.

```mermaid
sequenceDiagram
    participant User
    participant GraphBuilder
    participant GraphCore
    participant Executor

    User->>GraphBuilder: build()
    GraphBuilder->>GraphCore: diagnostics
    GraphCore-->>GraphBuilder: ok (empty)
    GraphBuilder->>GraphCore: export
    GraphCore-->>GraphBuilder: empty graph
    GraphBuilder-->>User: ExecGraph (0 steps)

    User->>Executor: execute()
    Note over Executor: no steps to enqueue
    Note over Executor: immediately complete

    Executor-->>User: result
    Note over User: success=true
    Note over User: completed=[]
    Note over User: total_duration~=0
```

**Notes:**
- Empty graph is valid (no cycles, no errors)
- Execution completes immediately
- Result indicates success with zero steps

---

## Scenario 6: Single-Step Graph

Graph has exactly one step with no dependencies.

```mermaid
sequenceDiagram
    participant User
    participant Executor
    participant ReadyQueue
    participant Worker
    participant Step

    User->>Executor: execute()
    Executor->>ReadyQueue: enqueue(S)
    Worker->>ReadyQueue: pull
    ReadyQueue-->>Worker: S
    Worker->>Step: exec
    Step-->>Worker: ok
    Worker->>Executor: done(S)

    Note over Executor: all complete

    Executor-->>User: result
    Note over User: success=true
    Note over User: completed=[S]
```

**Notes:**
- Simplest non-empty case
- Single step is immediately ready (no predecessors)
- Completes after one worker execution

---

## Scenario 7: Wide Parallel Graph (Many Concurrent Steps)

Graph: A -> {B1, B2, B3, B4} -> C (fan-out, fan-in)

```mermaid
sequenceDiagram
    participant User
    participant Executor
    participant ReadyQueue
    participant Worker1
    participant Worker2
    participant Worker3
    participant Worker4

    User->>Executor: execute()
    Executor->>ReadyQueue: enqueue(A)
    Worker1->>ReadyQueue: pull
    ReadyQueue-->>Worker1: A
    Note over Worker1: exec A
    Worker1->>Executor: done(A)

    Note over Executor: A done, B1-B4 ready
    Executor->>ReadyQueue: enqueue(B1,B2,B3,B4)

    par All 4 execute concurrently
        Worker1->>ReadyQueue: pull
        ReadyQueue-->>Worker1: B1
        Note over Worker1: exec B1
    and
        Worker2->>ReadyQueue: pull
        ReadyQueue-->>Worker2: B2
        Note over Worker2: exec B2
    and
        Worker3->>ReadyQueue: pull
        ReadyQueue-->>Worker3: B3
        Note over Worker3: exec B3
    and
        Worker4->>ReadyQueue: pull
        ReadyQueue-->>Worker4: B4
        Note over Worker4: exec B4
    end

    Worker1->>Executor: done(B1)
    Worker3->>Executor: done(B3)
    Worker2->>Executor: done(B2)
    Worker4->>Executor: done(B4)

    Note over Executor: all B's done, C ready
    Executor->>ReadyQueue: enqueue(C)
    Worker1->>ReadyQueue: pull
    ReadyQueue-->>Worker1: C
    Note over Worker1: exec C
    Worker1->>Executor: done(C)

    Executor-->>User: result
    Note over User: success=true
```

**Notes:**
- Multiple workers can execute concurrently
- B1-B4 all become ready when A completes (fan-out)
- C waits for all B's to complete (fan-in / join)
- Worker assignment is non-deterministic

---

## Scenario 8: Deep Sequential Graph (Long Dependency Chain)

Graph: A -> B -> C -> D -> E -> F (6 steps in sequence)

```mermaid
sequenceDiagram
    participant User
    participant Executor
    participant ReadyQueue
    participant Worker

    User->>Executor: execute()

    loop For each step A through F
        Executor->>ReadyQueue: enqueue(step)
        Worker->>ReadyQueue: pull
        ReadyQueue-->>Worker: step
        Note over Worker: exec step
        Worker->>Executor: done(step)
    end

    Executor-->>User: result
    Note over User: success=true
    Note over User: completed=[A,B,C,D,E,F]
```

**Expanded view:**

```mermaid
sequenceDiagram
    participant User
    participant Executor
    participant ReadyQueue
    participant Worker

    User->>Executor: execute()

    Executor->>ReadyQueue: enqueue(A)
    Worker->>ReadyQueue: pull
    Note over Worker: exec A
    Worker->>Executor: done(A)

    Executor->>ReadyQueue: enqueue(B)
    Worker->>ReadyQueue: pull
    Note over Worker: exec B
    Worker->>Executor: done(B)

    Executor->>ReadyQueue: enqueue(C)
    Worker->>ReadyQueue: pull
    Note over Worker: exec C
    Worker->>Executor: done(C)

    Executor->>ReadyQueue: enqueue(D)
    Worker->>ReadyQueue: pull
    Note over Worker: exec D
    Worker->>Executor: done(D)

    Executor->>ReadyQueue: enqueue(E)
    Worker->>ReadyQueue: pull
    Note over Worker: exec E
    Worker->>Executor: done(E)

    Executor->>ReadyQueue: enqueue(F)
    Worker->>ReadyQueue: pull
    Note over Worker: exec F
    Worker->>Executor: done(F)

    Executor-->>User: result
    Note over User: success=true
    Note over User: completed=[A,B,C,D,E,F]
```

**Notes:**
- No parallelism possible (each step depends on previous)
- Multiple workers won't help throughput
- Demonstrates worst-case for parallel execution
- Total time = sum of individual step times

---

## Summary: Graph-Level State Transitions

```mermaid
stateDiagram-v2
    [*] --> BUILDING

    BUILDING : GraphBuilder
    VALIDATING : GraphCore
    EXECUTABLE : ExecGraph
    EXECUTING : Executor

    BUILDING --> VALIDATING : build()

    VALIDATING --> FAILED_BUILD : errors
    VALIDATING --> EXECUTABLE : ok

    FAILED_BUILD --> [*] : throw

    EXECUTABLE --> EXECUTING : execute()

    EXECUTING --> STOPPED : stop
    EXECUTING --> COMPLETE : done
    EXECUTING --> FAILED_EXEC : error

    STOPPED --> [*]
    COMPLETE --> [*]
    FAILED_EXEC --> [*]
```

---

## References

- `2026-01-13_095409_execution_interaction_graph_level.md` - ASCII version of this document
- `2026-01-12_203411_task_execution_roadmap.md` - Execution phases
- `2026-01-12_215134_design_defects_and_pitfalls.md` - Known issues
