# 2026-01-13_095411_execution_interaction_data_flow.md

## Header

- **Title:** Execution Interaction Diagrams: Data Flow
- **Author:** Claude (AI) with human direction
- **Date:** 2026-01-13 (America/Los_Angeles)
- **Document Status:** Active
- **Related:**
  - `2026-01-13_095409_execution_interaction_graph_level.md`
  - `2026-01-13_095410_execution_interaction_step_level.md`
  - `2026-01-12_203411_task_execution_roadmap.md`

---

## Purpose

This document illustrates how data flows through the execution system, including:
- VarData lifecycle (creation, access, destruction)
- CrdToken authorization mechanism
- Create/Read/Destroy access patterns
- Concurrent access scenarios

**Note:** These diagrams are design exploration, not authoritative specifications.

---

## Key Components

| Component | Responsibility |
|-----------|----------------|
| **VarData** | Type-erased value storage; owned by Data |
| **Data (IData impl)** | Manages VarData; handles token validation and synchronization |
| **CrdToken** | Authorization token assigned to steps for data access |
| **Field (IField impl)** | Step's typed reference to Data; specifies Usage |
| **Usage** | Create, Read, or Destroy access pattern |

---

## VarData State Machine

```
           +-------+
           | Empty |
           +---+---+
               |
               | emplace<T>(value)
               v
         +-----+-----+
         | Populated |<----+
         +-----+-----+     |
              /|\          |
             / | \         |
   get<T>() /  |  \ as<T>()|  (read access, no state change)
           /   |   \       |
          +    |    +------+
               |
               | release<T>() or reset()
               v
           +---+---+
           | Empty |
           +-------+
```

**Invariants:**
- `(m_ti == typeid(void))` iff `m_pvoid == nullptr` (empty state)
- Type cannot change once populated (must reset first)

---

## Scenario 1: Create-Read-Destroy (Simple Linear)

Three steps: Creator -> Reader -> Destroyer

```
Creator    Field_C    Data    VarData    Field_R    Reader    Field_D    Destroyer
   |          |        |         |          |          |          |          |
   |          |        |      [Empty]       |          |          |          |
   |          |        |         |          |          |          |          |
   |--execute()        |         |          |          |          |          |
   |          |        |         |          |          |          |          |
   |--get_data()-->|   |         |          |          |          |          |
   |<--Data--------|   |         |          |          |          |          |
   |          |        |         |          |          |          |          |
   |--set_value(token, val)---->|          |          |          |          |
   |          |        |--emplace(val)---->|          |          |          |
   |          |        |         |     [Populated]    |          |          |
   |          |        |<--ok----|         |          |          |          |
   |<--ok------|--------|         |          |          |          |          |
   |          |        |         |          |          |          |          |
   | [Creator done]    |         |          |          |          |          |
   |          |        |         |          |          |          |          |
   |          |        |         |          |--execute()         |          |
   |          |        |         |          |          |          |          |
   |          |        |         |          |--get_data()-->|    |          |
   |          |        |         |          |<--Data--------|    |          |
   |          |        |         |          |          |          |          |
   |          |        |         |          |--get_value(token)->|          |
   |          |        |         |<--get<T>()----------|          |          |
   |          |        |         |--shared_ptr<T>----->|          |          |
   |          |        |<--VarData(copy)---|          |          |          |
   |          |        |         |     [Populated]    |          |          |
   |          |        |         |          |          |          |          |
   | [Reader done, value consumed]         |          |          |          |
   |          |        |         |          |          |          |          |
   |          |        |         |          |          |          |--execute()
   |          |        |         |          |          |          |          |
   |          |        |         |          |          |--get_data()-->|    |
   |          |        |         |          |          |<--Data--------|    |
   |          |        |         |          |          |          |          |
   |          |        |         |          |          |--remove_value(token)>
   |          |        |         |<--release<T>()-----|          |          |
   |          |        |         |--shared_ptr<T>---->|          |          |
   |          |        |         |      [Empty]       |          |          |
   |          |        |<--VarData(moved)--|          |          |          |
   |          |        |         |          |          |          |          |
   | [Destroyer done, value destroyed]     |          |          |          |
```

---

## Scenario 2: Create-Read-Read-Destroy (Multiple Readers)

One creator, two concurrent readers, one destroyer.

```
Creator    Data    VarData    Reader1    Reader2    Destroyer
   |        |         |          |          |           |
   |        |      [Empty]       |          |           |
   |        |         |          |          |           |
   |--set_value(token_C, val)-->|          |           |
   |        |--emplace(val)---->|          |           |
   |        |         |     [Populated]    |           |
   |<--ok---|         |          |          |           |
   |        |         |          |          |           |
   | [Creator done, R1 and R2 become ready]|           |
   |        |         |          |          |           |
   |        |         |          |--get_value(token_R1)|
   |        |         |          |          |--get_value(token_R2)
   |        |         |          |          |           |
   |        |--get<T>()-------->|          |           |
   |        |--get<T>()------------------- >|          |
   |        |         |          |          |           |
   |        |<--sptr--|          |          |           |
   |        |<--sptr-------------|          |           |
   |        |         |          |          |           |
   |        |         |  [Both readers have shared_ptr<T>]
   |        |         |  [VarData still Populated]     |
   |        |         |  [Concurrent reads are safe]   |
   |        |         |          |          |           |
   | [R1 done]        |          |          |           |
   | [R2 done]        |          |          |           |
   |        |         |          |          |           |
   |        |         |          |          |--remove_value(token_D)
   |        |--release<T>()---->|          |           |
   |        |         |      [Empty]       |           |
   |        |<--sptr--|         |          |           |
   |        |         |          |          |           |
   | [Destroyer done] |          |          |           |
```

**Notes:**
- `get<T>()` returns `shared_ptr<T>`, allowing multiple readers to hold references
- VarData remains Populated until `release<T>()` or `reset()`
- Readers' shared_ptrs keep the value alive even after release (if still held)

---

## Scenario 3: Concurrent Read Access (Parallel Readers)

Two readers execute simultaneously on different threads.

```
Thread1 (Reader1)    Data    VarData    Thread2 (Reader2)
       |               |        |              |
       |               |   [Populated]         |
       |               |        |              |
       |--get_value(t1)->      |              |
       |               |        |<--get_value(t2)
       |               |        |              |
       |    [Data.get_value acquires shared lock]
       |               |        |              |
       |               |--get<T>()             |
       |               |<--sptr-|              |
       |               |        |--get<T>()    |
       |               |        |<--sptr-------|
       |               |        |              |
       |<--VarData-----|        |              |
       |               |        |--VarData---->|
       |               |        |              |
       |    [Both threads have copies]         |
       |    [No data race - reads are safe]    |
       |               |        |              |
```

**Thread Safety Model:**
- Data (IData impl) holds synchronization primitive
- VarData itself has no mutex (value-like, safe to copy)
- Multiple `get_value()` calls can proceed concurrently
- `set_value()` and `remove_value()` require exclusive access

---

## Scenario 4: Token Authorization Success

Step's token matches authorized access rights.

```
GraphBuilder    ExecutableGraph    Step    Field    Data    VarData
     |                |             |        |        |        |
     |--build()       |             |        |        |        |
     |                |             |        |        |        |
     |--[for each step, assign token]        |        |        |
     |--[for each data, record (step, usage) rights]  |        |
     |                |             |        |        |        |
     |--[token_map: step_idx -> token]       |        |        |
     |--[access_rights: data_idx -> [(step_idx, Usage), ...]]  |
     |                |             |        |        |        |
     |                |             |        |        |        |
     [EXECUTION]      |             |        |        |        |
     |                |             |        |        |        |
     |                |             |--execute()      |        |
     |                |             |        |        |        |
     |                |             |--get_data()---->|        |
     |                |             |<--Data---------|        |
     |                |             |        |        |        |
     |                |             |--set_value(token, val)-->|
     |                |             |        |        |        |
     |                |             |        |--validate(token, Create)
     |                |             |        |  [lookup token in access_rights]
     |                |             |        |  [found: (step_idx, Create)]
     |                |             |        |  [token matches, Usage matches]
     |                |             |        |<--ok---|        |
     |                |             |        |        |        |
     |                |             |        |--emplace(val)-->|
     |                |             |        |        |   [Populated]
     |                |             |<--ok---|--------|        |
```

---

## Scenario 5: Token Authorization Rejection (Wrong Token)

Step attempts to access Data with unauthorized token.

```
Step_A(Create)    Step_B(Read)    Data    access_rights
      |                |           |           |
      |                |           |    [(A, Create), (B, Read)]
      |                |           |           |
      |--set_value(token_A, val)-->|           |
      |                |           |--validate(token_A, Create)
      |                |           |  [ok: A has Create right]
      |                |           |           |
      |<--ok-----------|           |           |
      |                |           |           |
      |                |--set_value(token_B, val)-->
      |                |           |           |
      |                |           |--validate(token_B, Create)
      |                |           |  [FAIL: B only has Read right]
      |                |           |  [B cannot Create]
      |                |           |           |
      |                |<--THROW---|           |
      |                |  UnauthorizedAccessError
      |                |  "Step B not authorized for Create on Data X"
```

**Notes:**
- Token validation happens at Data (IData impl) level
- Mismatch between step's token and required Usage triggers error
- This should not happen if graph was validated correctly
- Acts as defense-in-depth during execution

---

## Scenario 6: Access to Empty Data (MissingCreate Edge Case)

Step tries to read data that was never created.

```
Step_R(Read)    Data    VarData
      |          |         |
      |          |      [Empty]   <- No creator ever ran
      |          |         |
      |--get_value(token)-->
      |          |         |
      |          |--check state
      |          |  [VarData is Empty]
      |          |         |
      |<--THROW--|         |
      |  DataNotInitializedError
      |  "Data X has no value (missing Create)"
```

**Notes:**
- This scenario should be prevented by validation (MissingCreate diagnostic)
- If reached during execution, it's a defense-in-depth failure
- Alternative: Return `std::optional<VarData>` or `nullptr` shared_ptr

---

## Scenario 7: Double Destroy Attempt

Two steps both try to destroy the same data.

```
Step_D1(Destroy)    Step_D2(Destroy)    Data    VarData
      |                   |               |         |
      |                   |               |    [Populated]
      |                   |               |         |
      |--remove_value(token_D1)---------->|         |
      |                   |               |--release<T>()
      |                   |               |         |
      |<--VarData(val)----|               |    [Empty]
      |                   |               |         |
      |  [D1 successfully destroyed]      |         |
      |                   |               |         |
      |                   |--remove_value(token_D2)>|
      |                   |               |         |
      |                   |               |--check state
      |                   |               |  [VarData is Empty]
      |                   |               |  [Already destroyed!]
      |                   |               |         |
      |                   |<--THROW-------|         |
      |                   |  DoubleDestroyError
      |                   |  "Data X already destroyed"
```

**Notes:**
- Should be prevented by validation (MultipleDestroy diagnostic)
- If reached, Data tracks destruction state separately from VarData
- Alternative: Return empty VarData silently (idempotent destroy)

---

## Scenario 8: VarData Type Mismatch at Runtime

Step attempts to access data as wrong type.

```
Step_C(Create)    Step_R(Read)    Data    VarData
      |                |           |         |
      |--set_value(token, int{42})-->        |
      |                |           |--emplace<int>(42)
      |                |           |         |  [type_index = int]
      |<--ok-----------|           |    [Populated<int>]
      |                |           |         |
      |                |--get_value(token)-->|
      |                |           |         |
      |                |--as<string>()------>|
      |                |           |         |
      |                |           |  [type_index == int, not string]
      |                |           |         |
      |                |<--THROW---|         |
      |                |  std::bad_cast
      |                |  "VarData type mismatch: stored int, requested string"
```

**Notes:**
- Type mismatch should be prevented by validation (TypeMismatch diagnostic)
- Fields carry type_index; linked fields must have same type
- Runtime check is defense-in-depth
- Use `get<T>()` to return nullptr instead of throwing

---

## Scenario 9: VarData release() and Ownership Transfer

Data ownership transferred from Data to Destroyer step.

```
Data    VarData    Destroyer    Cleanup
  |        |          |           |
  |   [Populated]     |           |
  |   [refcount=1]    |           |
  |        |          |           |
  |--remove_value(token)--------->|
  |        |          |           |
  |--release<T>()---->|           |
  |        |          |           |
  |<--shared_ptr<T>---|           |
  |        |          |           |
  |   [Empty]         |           |
  |   [refcount still 1, held by Destroyer]
  |        |          |           |
  |        |          |--process value
  |        |          |           |
  |        |          |--[destructor called on exit]
  |        |          |           |
  |        |          |---------->|
  |        |          |   [shared_ptr<T> destroyed]
  |        |          |   [underlying T destroyed]
```

**Ownership Model:**
- `release<T>()` moves ownership out of VarData
- Caller becomes sole owner of the value
- VarData becomes Empty immediately
- Value's lifetime extends until caller releases shared_ptr

---

## Data Access Matrix

| Usage | set_value() | get_value() | remove_value() |
|-------|-------------|-------------|----------------|
| Create | ALLOWED | forbidden | forbidden |
| Read | forbidden | ALLOWED | forbidden |
| Destroy | forbidden | forbidden | ALLOWED |

**Enforcement points:**
1. Compile-time: Field's Usage determines which IData method to call
2. Validation-time: GraphCore checks Usage consistency
3. Runtime: Data validates token against access_rights

---

## Thread Safety for Data

```cpp
class Data : public IData {
private:
    std::shared_mutex m_mutex;  // Reader-writer lock
    VarData m_value;
    bool m_created{false};
    bool m_destroyed{false};
    // access_rights from ExecutableGraph

public:
    void set_value(CrdToken token, VarData value) override {
        validate(token, Usage::Create);
        std::unique_lock lock(m_mutex);  // Exclusive
        if (m_created) throw AlreadyCreatedError();
        m_value = std::move(value);
        m_created = true;
    }

    VarData get_value(CrdToken token) override {
        validate(token, Usage::Read);
        std::shared_lock lock(m_mutex);  // Shared
        if (!m_created) throw NotCreatedError();
        if (m_destroyed) throw AlreadyDestroyedError();
        return m_value;  // Copy
    }

    VarData remove_value(CrdToken token) override {
        validate(token, Usage::Destroy);
        std::unique_lock lock(m_mutex);  // Exclusive
        if (!m_created) throw NotCreatedError();
        if (m_destroyed) throw AlreadyDestroyedError();
        m_destroyed = true;
        return std::move(m_value);
    }
};
```

---

## Summary: Data Lifecycle

```
                    +-------+
                    | Empty |
                    +---+---+
                        |
                        | set_value() [Create step]
                        v
                  +-----+-----+
                  | Populated |<----+
                  +-----+-----+     |
                       /|\          |
                      / | \         |
         get_value() /  |  \        | (read access, no change)
                    /   |   \       |
                   +    |    +------+
                        |
                        | remove_value() [Destroy step]
                        v
                  +-----+-----+
                  | Destroyed |
                  +-----------+
                  (cannot access)
```

---

## References

- `2026-01-12_203411_task_execution_roadmap.md` - VarData design
- `2026-01-12_215134_design_defects_and_pitfalls.md` - Token validation gaps
- `2026-01-10_071521_dangers_of_invalid_graph_instantiation.md` - Execution dangers
