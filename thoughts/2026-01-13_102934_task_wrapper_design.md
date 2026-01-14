# 2026-01-13_102934_task_wrapper_design.md

## Header

- **Title:** TaskWrapper Design
- **Author:** Claude (AI) with human direction
- **Date:** 2026-01-13 (America/Los_Angeles)
- **Document Status:** Active
- **Related:**
  - `2026-01-12_203411_task_execution_roadmap.md`
  - `2026-01-13_095410_execution_interaction_step_level.md`
  - `2026-01-13_095409_execution_interaction_graph_level.md`

---

## Purpose

This document outlines the design of **TaskWrapper**, a framework-internal class that wraps user-defined `IStep` implementations for execution. TaskWrapper handles all orchestration concerns, keeping user code focused on business logic.

**Key insight from implementation experience:** The unit of work sent to a Worker is not the raw Step, but a wrapper that manages pre/post-execution interactions and directly participates in successor notification.

---

## Motivation

### Problem: Current Architecture Gap

The existing interaction diagrams show Workers calling `Step.execute()` directly, with completion notifications routed through the Executor. This centralizes too much responsibility in the Executor and obscures important framework logic.

### Solution: Decentralized Orchestration via TaskWrapper

TaskWrapper:
1. Encapsulates all framework orchestration around user code
2. Directly decrements predecessor counters on successors
3. Submits newly-ready successors to the queue
4. Reduces Executor to lifecycle management, not per-step coordination

---

## Responsibilities

### Pre-Execution

| Responsibility | Description |
|----------------|-------------|
| **State transition** | Queued → Executing |
| **Stop check** | Check `stop_requested` flag before starting |
| **Token binding** | Make CrdToken available for Data access |
| **Timing start** | Record start timestamp (if profiling enabled) |

### User Code Execution

| Responsibility | Description |
|----------------|-------------|
| **Invoke execute()** | Call `step->execute()` |
| **Exception boundary** | Catch all exceptions from user code |

### Post-Execution

| Responsibility | Description |
|----------------|-------------|
| **State transition** | Executing → Succeeded or Failed |
| **Timing end** | Record end timestamp, compute duration |
| **Exception capture** | Store exception_ptr for later retrieval |
| **Successor notification** | Decrement predecessor counters on successors |
| **Queue submission** | Submit newly-ready successors to ReadyQueue |
| **Completion signal** | Notify Executor of completion (for termination detection) |

---

## Class Structure

### Conceptual Interface

```cpp
// Illustrative - not authoritative

class TaskWrapper {
public:
    // Construction (by Executor during graph setup)
    TaskWrapper(
        StepPtr step,
        CrdToken token,
        size_t predecessor_count,
        std::weak_ptr<Executor> executor
    );

    // Successor linkage (after all wrappers created)
    void add_successor(std::weak_ptr<TaskWrapper> successor);

    // Called by Worker
    void run();

    // State inspection
    StepState state() const;
    bool is_ready() const;  // predecessor_count == 0

    // Result inspection (after completion)
    std::exception_ptr exception() const;
    std::chrono::nanoseconds duration() const;

    // Called by predecessor TaskWrappers
    // Returns true if this call made the task ready
    bool decrement_predecessor_count();

private:
    StepPtr m_step;
    CrdToken m_token;

    std::atomic<StepState> m_state{StepState::NotReady};
    std::atomic<size_t> m_predecessors_remaining;

    std::vector<std::weak_ptr<TaskWrapper>> m_successors;
    std::weak_ptr<Executor> m_executor;

    // Result storage
    std::exception_ptr m_exception;
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_end_time;
};
```

### Key Design Decisions

**Weak pointers to successors:** Prevents circular ownership. Executor owns all TaskWrappers; wrappers reference each other weakly.

**Weak pointer to Executor:** TaskWrapper needs access to ReadyQueue (owned by Executor) but shouldn't prevent Executor destruction.

**Atomic predecessor count:** Multiple predecessors may complete concurrently; decrement must be thread-safe.

**Atomic state:** State may be read while being transitioned; atomic ensures visibility.

---

## Ownership Model

```
┌─────────────────────────────────────────────────────────────┐
│                         Executor                             │
│  ┌─────────────────────────────────────────────────────┐    │
│  │  vector<shared_ptr<TaskWrapper>> m_tasks            │    │
│  │     ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐      │    │
│  │     │ TW_A │   │ TW_B │   │ TW_C │   │ TW_D │      │    │
│  │     └──┬───┘   └──┬───┘   └──┬───┘   └──────┘      │    │
│  │        │          │          │                      │    │
│  │        │ weak     │ weak     │ weak                 │    │
│  │        ▼          ▼          ▼                      │    │
│  │     successors: successors: successors:             │    │
│  │     [B,C]      [D]        [D]                       │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                              │
│  ┌─────────────────┐                                        │
│  │   ReadyQueue    │◄─── TaskWrappers submit ready tasks    │
│  └─────────────────┘                                        │
└─────────────────────────────────────────────────────────────┘
         ▲
         │ weak_ptr
         │
    ┌────┴────┐
    │ Workers │  (pull from queue, call wrapper.run())
    └─────────┘
```

**Ownership chain:**
- Executor owns all `shared_ptr<TaskWrapper>`
- TaskWrappers hold `weak_ptr` to each other (successors)
- TaskWrappers hold `weak_ptr` to Executor (for queue access)
- Workers don't own anything; they receive raw pointers or refs from queue

---

## Execution Flow

### run() Method Pseudocode

```cpp
void TaskWrapper::run() {
    // Pre-execution
    auto executor = m_executor.lock();
    if (!executor) return;  // Executor gone, abort

    if (executor->stop_requested()) {
        m_state.store(StepState::Cancelled);
        executor->notify_completion(this);
        return;
    }

    m_state.store(StepState::Executing);
    m_start_time = std::chrono::steady_clock::now();

    // User code execution
    try {
        m_step->execute();
        m_state.store(StepState::Succeeded);
    } catch (...) {
        m_exception = std::current_exception();
        m_state.store(StepState::Failed);
    }

    m_end_time = std::chrono::steady_clock::now();

    // Post-execution: notify successors
    for (auto& weak_succ : m_successors) {
        if (auto succ = weak_succ.lock()) {
            if (succ->decrement_predecessor_count()) {
                // This successor is now ready
                succ->m_state.store(StepState::Ready);
                executor->enqueue(succ);
            }
        }
    }

    // Signal completion to Executor
    executor->notify_completion(this);
}
```

### decrement_predecessor_count() Method

```cpp
bool TaskWrapper::decrement_predecessor_count() {
    size_t prev = m_predecessors_remaining.fetch_sub(1, std::memory_order_acq_rel);
    return (prev == 1);  // We were the last predecessor
}
```

---

## Interaction Diagram: Successor Notification

Decentralized notification where TaskWrapper directly notifies successors.

```
Worker    TaskWrapper_A    TaskWrapper_B    TaskWrapper_C    Executor    ReadyQueue
  |            |                |                |              |            |
  |--run()---->|                |                |              |            |
  |            |                |                |              |            |
  |            |--[state=Executing]              |              |            |
  |            |--[execute()]   |                |              |            |
  |            |--[state=Succeeded]              |              |            |
  |            |                |                |              |            |
  |            |--decrement()-->|                |              |            |
  |            |  [B.preds: 2->1]               |              |            |
  |            |<--false--------|                |              |            |
  |            |                |                |              |            |
  |            |--decrement()------------------>|              |            |
  |            |  [C.preds: 1->0]               |              |            |
  |            |<--true-------------------------|              |            |
  |            |                |                |              |            |
  |            |--[C is ready, state=Ready]---->|              |            |
  |            |--enqueue(C)------------------------------------------>     |
  |            |                |                |              |            |
  |            |--notify_completion()----------------------->  |            |
  |            |                |                |              |            |
  |<--return---|                |                |              |            |
```

**Key points:**
- A directly decrements B's and C's predecessor counts
- B not ready yet (has another predecessor)
- C becomes ready (count reached 0)
- A submits C to queue directly
- Executor only notified for termination detection

---

## Interaction Diagram: Join Point (Multiple Predecessors)

Step D depends on both B and C.

```
Worker1    TaskWrapper_B    Worker2    TaskWrapper_C    TaskWrapper_D    ReadyQueue
   |            |              |            |                |               |
   |--run()---->|              |            |                |               |
   |            |          run()            |                |               |
   |            |              |----------->|                |               |
   |            |              |            |                |               |
   |            |--[B succeeds]|            |                |               |
   |            |              |--[C succeeds]               |               |
   |            |              |            |                |               |
   |            |--decrement(D)-------------|--------------->|               |
   |            |  [D.preds: 2->1]          |                |               |
   |            |<--false------------------------------------|               |
   |            |              |            |                |               |
   |            |              |            |--decrement(D)->|               |
   |            |              |            |  [D.preds: 1->0]               |
   |            |              |            |<--true---------|               |
   |            |              |            |                |               |
   |            |              |            |--[D ready, state=Ready]        |
   |            |              |            |--enqueue(D)---------------->   |
   |            |              |            |                |               |
```

**Thread safety:**
- B and C may complete concurrently
- `fetch_sub` is atomic; exactly one caller gets `prev == 1`
- Only the "last predecessor" caller enqueues D
- No race, no double-enqueue

---

## Interaction Diagram: Failure Propagation

When a predecessor fails, what happens to successors?

**Policy decision (configurable):**

### Option A: Abort on First Failure (Default)

```
Worker    TaskWrapper_B(fails)    TaskWrapper_C    TaskWrapper_D    Executor
  |            |                       |                |              |
  |--run()---->|                       |                |              |
  |            |--[execute() throws]   |                |              |
  |            |--[state=Failed]       |                |              |
  |            |                       |                |              |
  |            |--notify_completion(failed)----------------------->    |
  |            |                       |                |              |
  |            |  [Executor sets abort flag]           |              |
  |            |  [Executor cancels pending tasks]     |              |
  |            |                       |                |              |
  |            |--[skip successor notification]        |              |
  |            |  [D stays NotReady, then Cancelled]   |              |
```

### Option B: Continue Independent Paths

```
Worker    TaskWrapper_B(fails)    TaskWrapper_C    TaskWrapper_D    Executor
  |            |                       |                |              |
  |--run()---->|                       |                |              |
  |            |--[execute() throws]   |                |              |
  |            |--[state=Failed]       |                |              |
  |            |                       |                |              |
  |            |--decrement(D)------------------------>|              |
  |            |  [D.preds: 2->1, but D won't run]     |              |
  |            |                       |                |              |
  |            |                       |--[C succeeds] |              |
  |            |                       |--decrement(D)>|              |
  |            |                       |  [D.preds: 1->0]             |
  |            |                       |                |              |
  |            |  [D ready but has failed predecessor] |              |
  |            |  [D marked DepFailed, not executed]   |              |
```

**Recommendation:** Start with Option A (abort on first failure). Option B requires tracking "any predecessor failed" state per task.

---

## State Transitions with TaskWrapper

Updated state machine showing TaskWrapper involvement:

```
                              TaskWrapper created
                                     |
                                     v
                              +------+------+
                              |  NotReady   |
                              |  (preds>0)  |
                              +------+------+
                                     |
                    predecessor.decrement_predecessor_count()
                    returns true (preds==0)
                                     |
                                     v
                              +------+------+
                              |    Ready    |
                              +------+------+
                                     |
                              enqueue to ReadyQueue
                                     |
                                     v
                              +------+------+
                              |   Queued    |
                              +------+------+
                                     |
                              Worker pulls, calls run()
                                     |
                                     v
                              +------+------+
                              | Executing   |
                              +------+------+
                                    /|\
                                   / | \
                         success  /  |  \ stop_requested
                                 /   |   \
                                v    |    v
                         +------+    |    +-----------+
                         |Succeed|   |    | Cancelled |
                         +------+    |    +-----------+
                                     |
                                fail |
                                     v
                              +------+------+
                              |   Failed    |
                              +-------------+
```

**Who performs each transition:**
- NotReady → Ready: `decrement_predecessor_count()` caller (a predecessor's run())
- Ready → Queued: Executor/predecessor's run() (via enqueue)
- Queued → Executing: Worker (via run())
- Executing → Succeeded/Failed/Cancelled: Worker (via run())

---

## Impact on Executor

With TaskWrapper handling per-step orchestration, Executor simplifies to:

### Executor Responsibilities

| Responsibility | Description |
|----------------|-------------|
| **Graph setup** | Create TaskWrappers, wire successor links |
| **Initial enqueue** | Enqueue steps with no predecessors |
| **Termination detection** | Count completions, detect all-done |
| **Stop coordination** | Set flag, wait for in-progress tasks |
| **Result aggregation** | Collect success/failure status |

### Executor Does NOT

- ~~Receive per-step completion notifications for successor wakeup~~
- ~~Manage predecessor counters~~
- ~~Decide which steps to enqueue next~~

These are now TaskWrapper responsibilities.

---

## Thread Safety Summary

| Operation | Synchronization | Notes |
|-----------|-----------------|-------|
| `m_predecessors_remaining` decrement | `atomic::fetch_sub` | Lock-free |
| `m_state` transitions | `atomic::store/load` | Lock-free |
| `m_successors` iteration | None needed | Immutable after setup |
| Queue submission | Queue's internal lock | Thread-safe queue |
| `notify_completion` | Executor's internal counter | Atomic increment |

**No mutexes in hot path.** All per-step synchronization uses atomics.

---

## Open Questions

### Q1: Should TaskWrapper be exposed in public API?

**Options:**
- Internal only (users see IStep, never TaskWrapper)
- Exposed for advanced use cases (custom executors)

**Recommendation:** Internal only initially.

### Q2: How to handle successor weak_ptr expiration?

If Executor is destroyed mid-execution (shouldn't happen in normal use):
- `m_executor.lock()` returns nullptr
- `run()` aborts early
- Successor weak_ptrs may also be expired

**Recommendation:** Document as undefined behavior; Executor must outlive execution.

### Q3: Should TaskWrapper own the CrdToken or reference ExecutableGraph's tokens?

**Options:**
- Copy token into TaskWrapper (simple, slight memory overhead)
- Reference into ExecutableGraph (requires ExecutableGraph lifetime guarantee)

**Recommendation:** Copy into TaskWrapper for simplicity.

### Q4: Timing granularity?

**Options:**
- Per-step timing always (slight overhead)
- Per-step timing optional (configured at execution start)
- No per-step timing (only total duration)

**Recommendation:** Optional, configured via `ExecutorConfig::collect_timing`.

---

## References

- `2026-01-12_203411_task_execution_roadmap.md` - Overall execution architecture
- `2026-01-13_095410_execution_interaction_step_level.md` - Step state transitions
- `2026-01-12_215134_design_defects_and_pitfalls.md` - Related open questions
