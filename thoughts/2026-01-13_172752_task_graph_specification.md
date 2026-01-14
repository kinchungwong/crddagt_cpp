# 2026-01-13_172752_task_graph_specification.md

- Author: Human (kinchungwong)
- Date: 2026-01-13 (America/Los_Angeles)
- Document status: Active

---

## Task Graph (crddagt) - Specification (Authoritative)

---

### Related documents

- [2026-01-08_094948_graph_core_design.md](2026-01-08_094948_graph_core_design.md) - Design process for GraphCore and related classes

---

### Terminology, concepts, and foundational assumptions

#### Step and Data

* Each piece of data is addressable.
* Each step is executable.
* Each step takes any number (from zero to any) of addressable data as input, either destructively (D for destroy) or non-destructively (R for read).
* Each step produces any number (from zero to any) of addressable data as output.

#### Acronyms

* DAG = directed acyclic graph, the order-of-execution relationship on executable steps.
* CRD = create, read, and destroy, the three kinds of operations on addressable data by executable steps.

#### Assumptions

* All steps execute with perfect reproducibility. Given the same data as input, execution produces the same output data.
* All steps execute without any potentially deadlocking operations, and without any potentially blocking behaviors.
* Each piece of data has a lifetime that can be definite, semi-definite, or indefinite.
  * The beginning can be either global (from the very beginning), or produced by a task (published at the end of the task).
  * The ending can be either global (remains available past the end), or destructively consumed by a task (unpublished at the start of the task).
  * If the data represents allocated resources that are scarce (such as memory), a resource release task can always be added.
* Steps can potentially be executed in parallel (overlapping) unless forbidden.
* Our knowledge of how steps use data can be modeled completely.
* Our theory of dependencies (data-use dependencies, order-of-execution dependencies) is complete.
* From our complete theory and knowledge of dependencies, we can induce a partially ordered relation on order-of-execution, which is the tight representation of the permissibility and non-permissibility for steps to execute overlapped (that is, in parallel).

---

### Foundational invariants

* Within this section, the word "scheduled" should be understood as the introduction of an order-of-execution dependency as an edge into a directed acyclic graph (DAG), and does not refer to any behaviors of a hypothetical scheduler.
* For each piece of addressable data in the model:
  * If the data exists in the model, either the data is provided upfront (global input), or else there must be a unique step (one and only one) that creates it (denoted C).
  * If there are steps that non-destructively reads it (denoted R), all read steps are scheduled strictly after the create step (C).
  * Each piece of addressable data allows for at most one step that destructively consumes it (denoted D). If such a step exists, it is scheduled strictly after the create step, and strictly after all read steps.
  * Optionally, for a piece of addressable data that does not have a step that destructively consumes it, it can be marked as being available as a global output, which can be retrieved after the completion of the task graph.
* The set of data-related order-of-execution dependencies are collectively known as the "implicit dependencies."
* In addition to implicit dependencies, explicit dependencies can be added between any steps, to account for either unmodeled dependencies (essential), or throughput-shaping optimizations (soft optional).
  * Explicit dependencies are not associated with any fields or data.
* The combination of implicit and explicit dependencies is simply called combined dependencies.
* The combined dependencies constitute a valid task graph if-and-only-if (iff) it is a DAG.

---

### Additional concepts needed by the framework

#### Field

* Field represents a Step's intention to use a piece of addressable Data.
* A Step can have any number of Fields.
* Fields can be linked together. Linking signifies that these Fields refer to the same piece of addressable Data.
* (Remark.) Field is a slightly generalized and relaxed concept of the Promise/Future pattern used in multithreaded programming.
* (Technique.) Union-Find is used to deduce the association between Field and Data.
* Analogy: if steps are like discrete electronic components, fields are their electrical connectors, and the links are wires.
* Analogy: the complete "netlist" is obtained by running Union-Find on all electrical connections (field links).

#### Executor

* A mechanism for parallel execution, and a facade that accepts parallelizable tasks to be executed in parallel.

#### VarData

* The actual storage class of type-erased data.
* Type information checked at runtime.
* (Related.) Fields are declared with type information, and these are checked at graph-building (field-linking) time.
* Steps access their addressable and type-specific data through the mediation of several classes.
  * (Access pattern.) Step -> Field -> Data -> VarData

#### GraphCore

* Manages the DAG and CRD representations without involving pointers and ownerships etc.

#### GraphBuilder

* Allows steps and fields to be added
* Allows links between steps (explicit)
* Allows links between fields (implicit, and forms an equivalence class on data accessed through fields)

#### GraphExecutor and TaskWrapper

* TaskWrapper acts as an adapter for Step.
* GraphExecutor is an orchestrator on top of Executor.
* Together, they accomplish the execution of a valid task graph.

---

### Graph validation and diagnostics

The validation and diagnostics of the DAG is classified into needs and wants. Needs are non-negotiable. Wants can be.

#### Needs

* An invalid task graph must not be allowed to proceed into execution.

#### Wants

* The user of the task graph framework will want to receive concise and actionable information on why a task graph is invalid, what went wrong, and when and where that happened.

---

### Graph Execution

#### Implementation of the execution backend

* The execution backend operates orthogonally to the graph; it is mostly graph-agnostic.
* An executor manages a task queue and a group of worker threads.
* Tasks that are ready to be executed are submitted to the executor, which are sent to the task queue.
* Each worker thread pulls from the task queue until they are commanded to shut down by the executor.

#### What is implied by the backend specification

* Any task that is submitted to the executor (and its task queue) can potentially start executing almost immediately, if a worker thread is available.
* Therefore, only tasks that have their preconditions fully satisfied should be allowed to be submitted.
* That said, the API for a task has no representation of preconditions.
* It is the submitter that has to be aware.

#### GraphExecutor and TaskWrapper's roles in execution

* TaskWrapper performs state maintenance steps before and after step execution.
* TaskWrapper notifies its successors (steps that depend on it) to decrement the counter of unsatisfied predecessors.
* When the count reaches zero, the step becomes ready to execute, and its TaskWrapper submits itself to the Executor and the task queue.
* In case of a thrown exception:
  * The exception is caught and saved in the TaskWrapper
  * The state is transitioned to Failure.
  * All successors will receive a notification to cancel.
  * In addition to this direct notification, the GraphExecutor and all TaskWrappers have shared access to a cancellation token (not the C++ standard library token).
  * This shared token is checked by each TaskWrapper immediately before starting.
  * If cancellation is asserted, TaskWrapper marks itself as Cancelled.
  * When a TaskWrapper ends with a Cancelled state, the sequence of notification is:
    * GraphExecutor shared cancellation token asserted.
    * Each successor receives direct cancellation. Each asserts its own cancellation flag.
    * Each successor is notified to decrement.
  * When the step wrapped by a TaskWrapper has already executed successfully, but the cancellation flag is asserted (graph-wide):
    * The TaskWrapper marks itself as Succeeded.
    * However, it sends direct cancellation to all successors.
    * Successors are also notified to decrement.

#### ExecutorManager

* ExecutorManager is a singleton object that manages one shared Executor.
* It can be explicitly commanded to bring up and shut down the Executor.
* By default, it does not spin up the Executor until the moment it is needed.
