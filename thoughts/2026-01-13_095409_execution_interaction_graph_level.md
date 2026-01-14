# 2026-01-13_095409_execution_interaction_graph_level.md

## Header

- **Title:** Execution Interaction Diagrams: Graph Level
- **Author:** Claude (AI) with human direction
- **Date:** 2026-01-13 (America/Los_Angeles)
- **Document Status:** Active
- **Related:**
  - `2026-01-13_095410_execution_interaction_step_level.md`
  - `2026-01-13_095411_execution_interaction_data_flow.md`
  - `2026-01-12_203411_task_execution_roadmap.md`

---

## Purpose

This document illustrates the graph-level execution lifecycle through chronological interaction diagrams. It covers the interactions between User code, GraphBuilder, GraphCore, Executor, ReadyQueue, and Workers.

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

```
User        GraphBuilder    GraphCore    ExecutableGraph    Executor    ReadyQueue    Worker
 |              |              |              |                |            |            |
 |              |              |              |                |            |            |
 |==== CONSTRUCTION PHASE ====|              |                |            |            |
 |              |              |              |                |            |            |
 |--add_step(A)>|              |              |                |            |            |
 |              |--add_step(0)>|              |                |            |            |
 |<----ok-------|              |              |                |            |            |
 |              |              |              |                |            |            |
 |--add_step(B)>|              |              |                |            |            |
 |              |--add_step(1)>|              |                |            |            |
 |<----ok-------|              |              |                |            |            |
 |              |              |              |                |            |            |
 |--link(A,B)-->|              |              |                |            |            |
 |              |--link(0,1)-->|              |                |            |            |
 |<----ok-------|              |              |                |            |            |
 |              |              |              |                |            |            |
 |... (add C, link B->C) ...  |              |                |            |            |
 |              |              |              |                |            |            |
 |==== BUILD PHASE ===========|              |                |            |            |
 |              |              |              |                |            |            |
 |--build()---->|              |              |                |            |            |
 |              |--diagnostics(sealed=true)-->|                |            |            |
 |              |<--DiagResult(no errors)-----|                |            |            |
 |              |              |              |                |            |            |
 |              |--export_graph()------------>|                |            |            |
 |              |<--ExportedGraph-------------|                |            |            |
 |              |              |              |                |            |            |
 |              |--[create ExecutableGraph]------------------>|            |            |
 |              |  [assign tokens, compute layers]            |            |            |
 |<--ExecGraph--|              |              |                |            |            |
 |              |              |              |                |            |            |
 |==== EXECUTION PHASE =======|              |                |            |            |
 |              |              |              |                |            |            |
 |--------------execute(graph)------------------------------>|            |            |
 |              |              |              |                |            |            |
 |              |              |              |                |--populate->|            |
 |              |              |              |       [A has no preds]      |            |
 |              |              |              |                |--enqueue(A)|            |
 |              |              |              |                |            |            |
 |              |              |              |                |            |<--pull-----|
 |              |              |              |                |            |--A-------->|
 |              |              |              |                |            |            |
 |              |              |              |                |   [Worker executes A]   |
 |              |              |              |                |            |            |
 |              |              |              |                |            |<--done(A)--|
 |              |              |              |                |<--notify(A complete)----|
 |              |              |              |                |            |            |
 |              |              |              |   [B's preds satisfied]     |            |
 |              |              |              |                |--enqueue(B)|            |
 |              |              |              |                |            |<--pull-----|
 |              |              |              |                |            |--B-------->|
 |              |              |              |                |   [Worker executes B]   |
 |              |              |              |                |            |<--done(B)--|
 |              |              |              |                |<--notify(B complete)----|
 |              |              |              |                |            |            |
 |              |              |              |   [C's preds satisfied]     |            |
 |              |              |              |                |--enqueue(C)|            |
 |              |              |              |                |            |<--pull-----|
 |              |              |              |                |            |--C-------->|
 |              |              |              |                |   [Worker executes C]   |
 |              |              |              |                |            |<--done(C)--|
 |              |              |              |                |<--notify(C complete)----|
 |              |              |              |                |            |            |
 |              |              |              |   [all steps complete]      |            |
 |              |              |              |                |            |            |
 |<--------------result(success)------------------------------|            |            |
```

---

## Scenario 2: Validation Failure at build()

Graph has a cycle (A->B->A), detected during build().

```
User        GraphBuilder    GraphCore
 |              |              |
 |--add_step(A)>|--add_step(0)>|
 |--add_step(B)>|--add_step(1)>|
 |--link(A,B)-->|--link(0,1)-->|
 |--link(B,A)-->|--link(1,0)-->|   [creates cycle]
 |              |              |
 |--build()---->|              |
 |              |--diagnostics(sealed=true)-->|
 |              |<--DiagResult(Cycle error)---|
 |              |              |
 |              |--[check has_errors()]
 |              |  [true: cycle detected]
 |              |              |
 |<--THROW------|              |
 |  GraphValidationError       |
 |  "Cycle detected: steps 0,1"|
 |              |              |
 |  [no ExecutableGraph created]
 |  [no execution occurs]      |
```

**Notes:**
- Validation happens eagerly at build() time
- User receives structured error with involved steps
- No resources allocated for execution

---

## Scenario 3: Step Failure During Execution

Step B throws an exception during execute().

```
User        Executor    ReadyQueue    Worker    StepA    StepB    StepC
 |              |            |           |         |        |        |
 |--execute()-->|            |           |         |        |        |
 |              |--enqueue(A)|           |         |        |        |
 |              |            |<--pull----|         |        |        |
 |              |            |--A------->|         |        |        |
 |              |            |           |--exec-->|        |        |
 |              |            |           |<--ok----|        |        |
 |              |<--done(A)--|-----------|         |        |        |
 |              |            |           |         |        |        |
 |              |--enqueue(B)|           |         |        |        |
 |              |            |<--pull----|         |        |        |
 |              |            |--B------->|         |        |        |
 |              |            |           |--exec--------->|        |
 |              |            |           |<--THROW!!------|        |
 |              |            |           |  std::runtime_error      |
 |              |            |           |         |        |        |
 |              |<--failed(B, error)-----|         |        |        |
 |              |            |           |         |        |        |
 |              |--[abort execution]     |         |        |        |
 |              |--[mark C as Cancelled] |         |        |        |
 |              |            |           |         |        |        |
 |              |--[collect results]     |         |        |        |
 |              |  success=false         |         |        |        |
 |              |  failed_steps=[1]      |         |        |        |
 |              |  error_messages=[...]  |         |        |        |
 |              |            |           |         |        |        |
 |<--result-----|            |           |         |        |        |
 |  success=false            |           |         |        |        |
 |  failed_steps=[B]         |           |         |        |        |
```

**Notes:**
- First failure triggers abort (configurable behavior in future)
- Pending steps (C) are marked Cancelled, not executed
- Worker catches exception, reports to Executor
- Result contains which step failed and error message

---

## Scenario 4: Stop Request (Graceful Shutdown)

User requests stop while execution is in progress.

```
User        Executor    ReadyQueue    Worker1    Worker2    StepA    StepB    StepC
 |              |            |           |           |         |        |        |
 |--execute()-->|            |           |           |         |        |        |
 |              |--enqueue(A)|           |           |         |        |        |
 |              |--enqueue(B)| [both ready, no deps] |         |        |        |
 |              |            |           |           |         |        |        |
 |              |            |<--pull----|           |         |        |        |
 |              |            |--A------->|           |         |        |        |
 |              |            |<--pull-------------- -|         |        |        |
 |              |            |--B------------------ >|         |        |        |
 |              |            |           |           |         |        |        |
 |              |            |           |--exec--->|         |        |        |
 |              |            |           |           |--exec--------- >|        |
 |              |            |           |           |         |        |        |
 |--request_stop()--------->|            |           |         |        |        |
 |              |--[set stop flag]       |           |         |        |        |
 |              |            |           |           |         |        |        |
 |              |            |           |<--ok-----|         |        |        |
 |              |<--done(A)--|-----------|           |         |        |        |
 |              |            |           |           |         |        |        |
 |              |            |           |           |<--ok------------ |        |
 |              |<--done(B)--|-------------------- --|         |        |        |
 |              |            |           |           |         |        |        |
 |              |--[check stop flag: true]          |         |        |        |
 |              |--[skip enqueuing C]    |           |         |        |        |
 |              |--[mark C as Cancelled] |           |         |        |        |
 |              |            |           |           |         |        |        |
 |<--result-----|            |           |           |         |        |        |
 |  success=false            |           |           |         |        |        |
 |  error="stopped by request"          |           |         |        |        |
 |  completed=[A,B]          |           |           |         |        |        |
 |  cancelled=[C]            |           |           |         |        |        |
```

**Notes:**
- In-progress steps (A, B) complete normally
- Pending steps (C) are not started, marked Cancelled
- Stop is cooperative, not preemptive
- Result indicates partial completion

---

## Scenario 5: Empty Graph Execution

Graph has no steps.

```
User        GraphBuilder    GraphCore    Executor
 |              |              |            |
 |--build()---->|              |            |
 |              |--diagnostics>|            |
 |              |<--ok (empty)-|            |
 |              |--export----->|            |
 |              |<--empty graph|            |
 |<--ExecGraph--|              |            |
 |   (0 steps)  |              |            |
 |              |              |            |
 |--execute()------------------------>      |
 |              |              |            |
 |              |              |   [no steps to enqueue]
 |              |              |   [immediately complete]
 |              |              |            |
 |<--result--------------------------       |
 |  success=true               |            |
 |  completed=[]               |            |
 |  total_duration~=0          |            |
```

**Notes:**
- Empty graph is valid (no cycles, no errors)
- Execution completes immediately
- Result indicates success with zero steps

---

## Scenario 6: Single-Step Graph

Graph has exactly one step with no dependencies.

```
User        Executor    ReadyQueue    Worker    Step
 |              |            |           |        |
 |--execute()-->|            |           |        |
 |              |--enqueue(S)|           |        |
 |              |            |<--pull----|        |
 |              |            |--S------->|        |
 |              |            |           |--exec->|
 |              |            |           |<--ok---|
 |              |<--done(S)--|-----------|        |
 |              |            |           |        |
 |              |--[all complete]        |        |
 |              |            |           |        |
 |<--result-----|            |           |        |
 |  success=true             |           |        |
 |  completed=[S]            |           |        |
```

**Notes:**
- Simplest non-empty case
- Single step is immediately ready (no predecessors)
- Completes after one worker execution

---

## Scenario 7: Wide Parallel Graph (Many Concurrent Steps)

Graph: A -> {B1, B2, B3, B4} -> C (fan-out, fan-in)

```
User        Executor    ReadyQueue    Worker1    Worker2    Worker3    Worker4
 |              |            |           |           |           |           |
 |--execute()-->|            |           |           |           |           |
 |              |--enqueue(A)|           |           |           |           |
 |              |            |<--pull----|           |           |           |
 |              |            |           |--exec A   |           |           |
 |              |<--done(A)--|-----------|           |           |           |
 |              |            |           |           |           |           |
 |              |--[A done, B1-B4 ready] |           |           |           |
 |              |--enqueue(B1,B2,B3,B4)  |           |           |           |
 |              |            |           |           |           |           |
 |              |            |<--pull----|           |           |           |
 |              |            |--B1------>|           |           |           |
 |              |            |<--pull-------------- -|           |           |
 |              |            |--B2----------------->|           |           |
 |              |            |<--pull--------------------------  |           |
 |              |            |--B3------------------------------ >|          |
 |              |            |<--pull----------------------------------------|
 |              |            |--B4------------------------------------------ >|
 |              |            |           |           |           |           |
 |              |            |  [all 4 execute concurrently]     |           |
 |              |            |           |           |           |           |
 |              |<--done(B1)|-----------|           |           |           |
 |              |<--done(B3)|--------------------------- -------|           |
 |              |<--done(B2)|-------------------- --|           |           |
 |              |<--done(B4)|----------------------------------------------- |
 |              |            |           |           |           |           |
 |              |--[all B's done, C ready]          |           |           |
 |              |--enqueue(C)|           |           |           |           |
 |              |            |<--pull----|           |           |           |
 |              |            |           |--exec C   |           |           |
 |              |<--done(C)--|-----------|           |           |           |
 |              |            |           |           |           |           |
 |<--result-----|            |           |           |           |           |
 |  success=true             |           |           |           |           |
```

**Notes:**
- Multiple workers can execute concurrently
- B1-B4 all become ready when A completes (fan-out)
- C waits for all B's to complete (fan-in / join)
- Worker assignment is non-deterministic

---

## Scenario 8: Deep Sequential Graph (Long Dependency Chain)

Graph: A -> B -> C -> D -> E -> F (6 steps in sequence)

```
User        Executor    ReadyQueue    Worker
 |              |            |           |
 |--execute()-->|            |           |
 |              |--enqueue(A)|           |
 |              |            |<--pull----|
 |              |            |           |--exec A
 |              |<--done(A)--|-----------|
 |              |--enqueue(B)|           |
 |              |            |<--pull----|
 |              |            |           |--exec B
 |              |<--done(B)--|-----------|
 |              |--enqueue(C)|           |
 |              |            |<--pull----|
 |              |            |           |--exec C
 |              |<--done(C)--|-----------|
 |              |--enqueue(D)|           |
 |              |            |<--pull----|
 |              |            |           |--exec D
 |              |<--done(D)--|-----------|
 |              |--enqueue(E)|           |
 |              |            |<--pull----|
 |              |            |           |--exec E
 |              |<--done(E)--|-----------|
 |              |--enqueue(F)|           |
 |              |            |<--pull----|
 |              |            |           |--exec F
 |              |<--done(F)--|-----------|
 |              |            |           |
 |<--result-----|            |           |
 |  success=true             |           |
 |  completed=[A,B,C,D,E,F]  |           |
```

**Notes:**
- No parallelism possible (each step depends on previous)
- Multiple workers won't help throughput
- Demonstrates worst-case for parallel execution
- Total time = sum of individual step times

---

## Summary: Graph-Level State Transitions

```
              +-------------+
              |   BUILDING  |
              | (GraphBuilder)|
              +------+------+
                     | build()
                     v
              +------+------+
              |  VALIDATING |
              | (GraphCore)  |
              +------+------+
                    /  \
           errors /    \ ok
                 v      v
        +--------+    +-+----------+
        | FAILED |    | EXECUTABLE |
        | (throw)|    | (ExecGraph)|
        +--------+    +-----+------+
                            | execute()
                            v
                     +------+------+
                     |  EXECUTING  |
                     |  (Executor) |
                     +------+------+
                     /      |      \
              stop /   done |       \ error
                  v        v        v
           +------+  +-----+----+ +------+
           |STOPPED| | COMPLETE | |FAILED|
           +------+  +----------+ +------+
```

---

## References

- `2026-01-12_203411_task_execution_roadmap.md` - Execution phases
- `2026-01-12_215134_design_defects_and_pitfalls.md` - Known issues
