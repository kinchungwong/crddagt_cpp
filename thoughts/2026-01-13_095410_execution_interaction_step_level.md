# 2026-01-13_095410_execution_interaction_step_level.md

## Header

- **Title:** Execution Interaction Diagrams: Step Level
- **Author:** Claude (AI) with human direction
- **Date:** 2026-01-13 (America/Los_Angeles)
- **Document Status:** Active
- **Related:**
  - `2026-01-13_095409_execution_interaction_graph_level.md`
  - `2026-01-13_095411_execution_interaction_data_flow.md`
  - `2026-01-12_203411_task_execution_roadmap.md`

---

## Purpose

This document illustrates step-level state transitions and interactions. It focuses on how individual steps move through their lifecycle and how predecessor/successor notifications flow.

**Note:** These diagrams are design exploration, not authoritative specifications.

---

## StepState Enum

From the roadmap (not authoritative):

```cpp
enum class StepState {
    NotReady,    // Waiting for predecessors
    Ready,       // All predecessors complete, eligible for queue
    Queued,      // In the ready queue, waiting for worker
    Executing,   // Worker is running execute()
    Succeeded,   // execute() completed normally
    Failed,      // execute() threw exception
    Cancelled    // Execution aborted before starting
};
```

---

## State Machine Diagram

```
                         +------------+
                         |  NotReady  |
                         +-----+------+
                               |
                               | all predecessors complete
                               v
                         +-----+------+
                         |   Ready    |
                         +-----+------+
                               |
                               | enqueued to ReadyQueue
                               v
                         +-----+------+
                         |   Queued   |
                         +-----+------+
                               |
                               | worker picks up
                               v
                         +-----+------+
                         | Executing  |
                         +-----+------+
                              /|\
                             / | \
                  success   /  |  \   stop_requested
                           /   |   \  (before start)
                          v    |    v
                   +------+    |    +----------+
                   |Succeed|   |    | Cancelled|
                   +------+    |    +----------+
                               |          ^
                          fail |          |
                               v          | stop_requested
                         +-----+------+   | (while NotReady/Ready/Queued)
                         |   Failed   |   |
                         +------------+---+
```

---

## Scenario 1: Normal State Progression (Happy Path)

Step transitions through all states successfully.

```
Executor    ReadyQueue    Worker    Step    Step.state
   |            |           |        |          |
   |            |           |        |     [NotReady]
   |            |           |        |          |
   |--[predecessor completes]        |          |
   |            |           |        |          |
   |--check_ready(S)------->|        |          |
   |  [all preds done]      |        |          |
   |            |           |        |          |
   |--set_state(Ready)-------------->|          |
   |            |           |        |<---------|
   |            |           |        |      [Ready]
   |            |           |        |          |
   |--enqueue(S)>|          |        |          |
   |            |           |        |          |
   |--set_state(Queued)------------->|          |
   |            |           |        |<---------|
   |            |           |        |     [Queued]
   |            |           |        |          |
   |            |<--pull----|        |          |
   |            |--S------->|        |          |
   |            |           |        |          |
   |            |           |--set_state(Executing)->|
   |            |           |        |<---------|
   |            |           |        |   [Executing]
   |            |           |        |          |
   |            |           |--execute()------->|
   |            |           |        |  [work]  |
   |            |           |<--return---------|
   |            |           |        |          |
   |            |           |--set_state(Succeeded)->|
   |            |           |        |<---------|
   |            |           |        |   [Succeeded]
   |            |           |        |          |
   |<--notify_complete(S)---|        |          |
```

---

## Scenario 2: Step Failure (Executing -> Failed)

Step throws exception during execute().

```
Worker    Step    Step.state    Executor
  |        |          |            |
  |--set_state(Executing)-------->|
  |        |<---------|            |
  |        |    [Executing]        |
  |        |          |            |
  |--execute()------->|            |
  |        |   [work begins]       |
  |        |   [exception!]        |
  |<--THROW-----------|            |
  |  std::runtime_error            |
  |        |          |            |
  |--[catch exception]             |
  |        |          |            |
  |--set_state(Failed)------------>|
  |        |<---------|            |
  |        |    [Failed]           |
  |        |          |            |
  |--notify_failed(S, ex)--------->|
  |        |          |            |
  |        |          |  [Executor records failure]
  |        |          |  [triggers abort if configured]
```

**Notes:**
- Worker catches the exception, doesn't propagate
- Step state captures the failure
- Executor decides whether to abort or continue

---

## Scenario 3: Cancellation Propagation

Stop requested while steps are in various states.

```
Executor    Step_A(Executing)    Step_B(Queued)    Step_C(Ready)    Step_D(NotReady)
   |              |                   |                 |                |
   |         [Executing]          [Queued]          [Ready]         [NotReady]
   |              |                   |                 |                |
   |--[stop_requested]                |                 |                |
   |              |                   |                 |                |
   |  [A continues - in progress]     |                 |                |
   |              |                   |                 |                |
   |--cancel(B)---------------------->|                 |                |
   |              |              [Cancelled]            |                |
   |              |                   |                 |                |
   |--cancel(C)---------------------------------------->|                |
   |              |                   |            [Cancelled]           |
   |              |                   |                 |                |
   |--cancel(D)--------------------------------------------------------->|
   |              |                   |                 |           [Cancelled]
   |              |                   |                 |                |
   |<--done(A)----|                   |                 |                |
   |         [Succeeded]              |                 |                |
   |              |                   |                 |                |
   |  [A completed, B/C/D cancelled]  |                 |                |
```

**State Transition Rules for Cancellation:**
- `NotReady` -> `Cancelled`: Immediate
- `Ready` -> `Cancelled`: Immediate (not yet queued)
- `Queued` -> `Cancelled`: Remove from queue, mark cancelled
- `Executing` -> (complete normally): Cannot cancel mid-execution
- `Succeeded/Failed` -> (no change): Terminal states

---

## Scenario 4: Predecessor Completion Notification

Step waits for multiple predecessors; each completion triggers a check.

```
Executor    Step_A    Step_B    Step_C(needs A,B)    Step_C.preds_remaining
   |          |         |              |                    [2]
   |          |         |              |                     |
   |--execute A         |              |                     |
   |          |         |              |                     |
   |<-done(A)-|         |              |                     |
   |          |         |              |                     |
   |--notify_pred_complete(C, A)------>|                     |
   |          |         |              |--decrement--------->|
   |          |         |              |                    [1]
   |          |         |              |                     |
   |  [preds_remaining > 0, still NotReady]                  |
   |          |         |              |                     |
   |--execute B         |              |                     |
   |          |         |              |                     |
   |<-done(B)-----------|              |                     |
   |          |         |              |                     |
   |--notify_pred_complete(C, B)------>|                     |
   |          |         |              |--decrement--------->|
   |          |         |              |                    [0]
   |          |         |              |                     |
   |  [preds_remaining == 0]           |                     |
   |          |         |              |                     |
   |--set_state(Ready)---------------->|                     |
   |          |         |         [Ready]                    |
   |--enqueue(C)       |              |                     |
```

**Notes:**
- `preds_remaining` is atomically decremented
- Only when count reaches 0 does step become Ready
- Order of predecessor completion doesn't matter

---

## Scenario 5: Successor Notification on Completion

Completed step notifies its successors.

```
Executor    Step_A    Step_B(succ of A)    Step_C(succ of A)
   |          |              |                    |
   |  [A executing]          |                    |
   |          |              |                    |
   |<-done(A)-|              |                    |
   |          |              |                    |
   |--[lookup successors of A]                    |
   |  [successors = {B, C}]  |                    |
   |          |              |                    |
   |--notify_pred_complete(B, A)-->|              |
   |          |              |--check_ready       |
   |          |              |                    |
   |--notify_pred_complete(C, A)----------------->|
   |          |              |              |--check_ready
   |          |              |                    |
   |  [if B ready, enqueue B]|                    |
   |  [if C ready, enqueue C]|                    |
```

**Notes:**
- Executor maintains successor lists from ExportedGraph
- All successors notified in sequence (or parallel with proper sync)
- Each successor independently checks if all its preds are done

---

## Scenario 6: Multiple Predecessors (Join Point)

Step D depends on A, B, and C (3 predecessors).

```
                    +---+
                    | A |
                    +-+-+
                      |
          +-----------+-----------+
          |           |           |
        +-+-+       +-+-+       +-+-+
        | B |       | C |       | D |---+
        +---+       +---+       +---+   |
                                  ^     |
                                  |     |
                      (depends on A,B,C)|
                                        v
                                    +---+---+
                                    | Result|
                                    +-------+
```

```
Time    A.state    B.state    C.state    D.state    D.preds
 t0     Ready      Ready      Ready      NotReady   [3]
 t1     Executing  Queued     Queued     NotReady   [3]
 t2     Succeeded  Executing  Executing  NotReady   [3]
 t3     Succeeded  Succeeded  Executing  NotReady   [2]  <- A done, B done
 t4     Succeeded  Succeeded  Succeeded  NotReady   [1]  <- C still running
 t5     Succeeded  Succeeded  Succeeded  Ready      [0]  <- C done, D ready
 t6     Succeeded  Succeeded  Succeeded  Queued     [0]
 t7     Succeeded  Succeeded  Succeeded  Executing  [0]
 t8     Succeeded  Succeeded  Succeeded  Succeeded  [0]
```

---

## Scenario 7: Multiple Successors (Fork Point)

Step A has successors B, C, and D (fan-out).

```
                    +---+
                    | A |
                    +-+-+
                      |
          +-----------+-----------+
          |           |           |
          v           v           v
        +-+-+       +-+-+       +-+-+
        | B |       | C |       | D |
        +---+       +---+       +---+
```

```
Executor    ReadyQueue    Worker1    Worker2    Worker3    A    B    C    D
   |            |           |           |           |      |    |    |    |
   |            |           |           |           |   [Exec] [NR] [NR] [NR]
   |            |           |           |           |      |    |    |    |
   |<--done(A)--|-----------|           |           |      |    |    |    |
   |            |           |           |           |   [Succ]  |    |    |
   |            |           |           |           |      |    |    |    |
   |--notify B, C, D (A complete)------------------------>|    |    |    |
   |            |           |           |           |      | [Rdy][Rdy][Rdy]
   |            |           |           |           |      |    |    |    |
   |--enqueue(B)|           |           |           |      |    |    |    |
   |--enqueue(C)|           |           |           |      |    |    |    |
   |--enqueue(D)|           |           |           |      |    |    |    |
   |            |           |           |           |      |    |    |    |
   |            |<--pull----|           |           |      |    |    |    |
   |            |<--pull-------------- -|           |      |    |    |    |
   |            |<--pull----------------------------|      |    |    |    |
   |            |           |           |           |      |    |    |    |
   |            |  [B,C,D execute concurrently]    |      |    |    |    |
```

**Notes:**
- Single predecessor completion can trigger multiple successors
- All become Ready simultaneously
- All can execute in parallel if workers available

---

## Scenario 8: Self-Loop Detection (Validation Prevents)

Self-loop A->A should be caught during validation, not execution.

```
User        GraphBuilder    GraphCore
  |              |              |
  |--add_step(A)>|--add_step(0)>|
  |              |              |
  |--link(A,A)-->|--link(0,0)-->|
  |              |              |
  |              |  [GraphCore detects self-loop]
  |              |  [Records Cycle diagnostic]
  |              |              |
  |--build()---->|              |
  |              |--diagnostics>|
  |              |<--Cycle(0)---|
  |              |              |
  |<--THROW------|              |
  |  "Self-loop on step 0"     |
  |              |              |
  |  [Execution never attempted]
```

**Notes:**
- Self-loops are a trivial cycle (SCC of size 1)
- Detected by GraphCore during validation
- If somehow reached execution, step would be NotReady forever (pred never completes)

---

## State Transition Summary Table

| Current State | Event | Next State | Condition |
|---------------|-------|------------|-----------|
| NotReady | pred_complete | NotReady | preds_remaining > 0 |
| NotReady | pred_complete | Ready | preds_remaining == 0 |
| NotReady | cancel | Cancelled | stop_requested |
| Ready | enqueue | Queued | - |
| Ready | cancel | Cancelled | stop_requested |
| Queued | worker_pickup | Executing | - |
| Queued | cancel | Cancelled | stop_requested |
| Executing | execute_return | Succeeded | no exception |
| Executing | execute_throw | Failed | exception caught |
| Executing | (none) | (none) | cannot cancel mid-exec |
| Succeeded | (none) | (none) | terminal |
| Failed | (none) | (none) | terminal |
| Cancelled | (none) | (none) | terminal |

---

## Thread Safety Considerations

**State transitions must be atomic:**
- Multiple predecessors may complete concurrently
- Multiple workers may race to pick up queued steps
- Stop request may arrive during any state

**Recommended implementation:**
```cpp
class StepStateManager {
    std::atomic<StepState> m_state{StepState::NotReady};
    std::atomic<size_t> m_preds_remaining;

    // Returns true if this call caused the transition to Ready
    bool notify_pred_complete() {
        size_t prev = m_preds_remaining.fetch_sub(1);
        if (prev == 1) {
            // We were the last predecessor
            m_state.store(StepState::Ready);
            return true;
        }
        return false;
    }
};
```

---

## References

- `2026-01-12_203411_task_execution_roadmap.md` - StepState enum definition
- `2026-01-13_095409_execution_interaction_graph_level.md` - Graph-level context
