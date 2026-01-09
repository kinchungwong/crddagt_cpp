# 2026-01-08_202059_iterable_union_find_design.md

- Author: Claude Opus 4.5 (Anthropic)
- Date: 2026-01-08 (America/Los_Angeles)
- Document status: Active

## See Also

- [2026-01-08_180853_eager_cycle_detection_research.md](2026-01-08_180853_eager_cycle_detection_research.md) - Parent research document (Implementation Plan Phases 1-3)
- [2026-01-08_185125_eager_cycle_detection_snippets.md](2026-01-08_185125_eager_cycle_detection_snippets.md) - Code snippets including union-find operations

## Purpose

Design document for a standalone `IterableUnionFind` class extracted from the embedded union-find implementation in `GraphCore`. This class combines:

- **Union-by-rank**: Keeps trees balanced for O(α(n)) amortized find operations
- **Path compression**: Flattens trees during find for efficiency
- **Exact size tracking**: Maintains class sizes with totality invariant
- **Circular linked list**: Enables O(class_size) enumeration of class members

## Motivation for Extraction

The union-find implementation in `GraphCore` has grown from a simple parent/rank structure into a full-featured component. Extracting it provides:

1. **Testability**: Isolated unit tests for the data structure itself
2. **Reusability**: Other graph algorithms may need similar functionality
3. **Clarity**: Separates concerns between equivalence tracking and graph semantics
4. **Assurance**: Critical infrastructure deserves dedicated verification

## Interface Design

### Class Template

```cpp
template <typename Idx = size_t>
class IterableUnionFind {
public:
    explicit IterableUnionFind() = default;

    // Element management
    Idx make_set();                                      // Create new singleton set, returns its index
    [[nodiscard]] size_t element_count() const;          // Total elements created

    // Core operations
    [[nodiscard]] Idx find(Idx x);                       // Find root (with path compression)
    bool unite(Idx a, Idx b);                            // Merge sets, returns true if merge occurred

    // Queries
    [[nodiscard]] size_t class_size(Idx x) const;        // Size of class containing x
    [[nodiscard]] Idx class_root(Idx x) const;           // Root without path compression (const)
    void get_class_members(Idx x, std::vector<Idx>& out) const;  // All members of class containing x
    [[nodiscard]] bool same_class(Idx a, Idx b) const;   // Are a and b in the same class?

private:
    void validate_index(Idx x) const;  // Throws std::runtime_error if x out of range

    std::vector<Idx> m_parent;    // Parent pointer (root if m_parent[x] == x)
    std::vector<size_t> m_rank;   // Tree rank for union-by-rank
    std::vector<size_t> m_size;   // Class size (valid only at root, 0 elsewhere)
    std::vector<Idx> m_next;      // Circular linked list for iteration
};
```

### Critical Design Notes

#### 1. Template Parameter `Idx`

The template parameter `Idx` is the index type, defaulting to `size_t`. This allows future type safety if `StepIdx` and `FieldIdx` become distinct types rather than aliases.

**Decision**: Keep as template for flexibility, but expect `size_t` in practice.

#### 2. `find()` is Non-Const (Path Compression)

Path compression modifies `m_parent` during lookup, so `find()` cannot be `const`. This is standard for union-find with path compression.

**Implication**: Callers needing const access must use `class_root()` instead, which traverses without compression.

#### 3. `class_size()` Must Internally Find Root

**Issue identified during design review**: The snippets show `m_field_uf_size[root]` being accessed, assuming the caller already has the root. But if `class_size(x)` is called where `x` is not the root, it would incorrectly return 0.

**Resolution**: `class_size()` must internally call `class_root()` to find the root before accessing size:

```cpp
[[nodiscard]] size_t class_size(Idx x) const {
    return m_size[class_root(x)];
}
```

This makes `class_size()` O(α(n)) rather than O(1), but correctness trumps micro-optimization.

#### 4. `get_class_members()` Works From Any Member

The circular linked list allows enumeration starting from any class member, not just the root. This is correct and intentional—every element is in the circular list.

#### 5. `unite()` Returns Bool for Idempotency Awareness

Returns `true` if a merge actually occurred, `false` if elements were already in the same class. This helps callers avoid redundant work.

## Data Structure Invariants

### INV1: Parent Tree Invariant

For all valid indices `x`:
- `m_parent[x]` is a valid index
- Following parent pointers eventually reaches a root where `m_parent[root] == root`

### INV2: Size Totality Invariant

Sum of all `m_size[x]` equals `element_count()`. This is maintained by:
- `make_set()`: Sets `m_size[x] = 1` for new element
- `unite()`: New root gets sum of both sizes; old root gets 0

### INV3: Circular List Completeness

For all valid indices `x`:
- Following `m_next` pointers from `x` visits exactly all members of `x`'s equivalence class
- The traversal returns to `x` after exactly `class_size(x)` steps

### INV4: Rank Bound

`m_rank[x]` is an upper bound on the height of the subtree rooted at `x`. This ensures union-by-rank keeps trees balanced.

**Note**: Rank and size are independent—rank bounds height, size counts members. They can diverge significantly.

## Algorithm Details

### `make_set()`

```cpp
Idx make_set() {
    Idx x = static_cast<Idx>(m_parent.size());
    m_parent.push_back(x);      // Self-parent (is own root)
    m_rank.push_back(0);        // Initial rank 0
    m_size.push_back(1);        // Singleton has size 1
    m_next.push_back(x);        // Self-loop (singleton circular list)
    return x;
}
```

### `find()` with Two-Pass Path Compression

**Decision**: Use iterative two-pass path compression to avoid stack overflow risks.

- **Pass 1**: Traverse from `x` to root, following parent pointers
- **Pass 2**: Traverse the same path again, rewriting each node's parent to point directly to the root

This two-pass approach is explicit and avoids recursion:

```cpp
Idx find(Idx x) {
    validate_index(x);

    // Pass 1: Find root
    Idx root = x;
    while (m_parent[root] != root) {
        root = m_parent[root];
    }

    // Pass 2: Path compression - rewrite parents to point to root
    while (m_parent[x] != root) {
        Idx next = m_parent[x];
        m_parent[x] = root;
        x = next;
    }

    return root;
}
```

### `class_root()` without Path Compression (Const)

```cpp
[[nodiscard]] Idx class_root(Idx x) const {
    validate_index(x);
    while (m_parent[x] != x) {
        x = m_parent[x];
    }
    return x;
}
```

### `unite()` with Union-by-Rank and List Splicing

```cpp
bool unite(Idx a, Idx b) {
    // validate_index called by find()
    Idx root_a = find(a);
    Idx root_b = find(b);

    if (root_a == root_b) {
        return false;  // Already in same class
    }

    // Compute combined size before modifying
    size_t combined_size = m_size[root_a] + m_size[root_b];

    // Union by rank
    Idx new_root, old_root;
    if (m_rank[root_a] < m_rank[root_b]) {
        m_parent[root_a] = root_b;
        new_root = root_b;
        old_root = root_a;
    } else if (m_rank[root_a] > m_rank[root_b]) {
        m_parent[root_b] = root_a;
        new_root = root_a;
        old_root = root_b;
    } else {
        m_parent[root_b] = root_a;
        m_rank[root_a]++;
        new_root = root_a;
        old_root = root_b;
    }

    // Update sizes
    m_size[new_root] = combined_size;
    m_size[old_root] = 0;

    // Splice circular lists at the roots for deterministic behavior
    std::swap(m_next[root_a], m_next[root_b]);

    return true;
}
```

**Design decision**: The circular list splice uses the **roots** (`root_a` and `root_b`), not the original arguments. While splicing at any two elements from different lists is mathematically correct, using the roots provides more deterministic behavior for debugging—the splice point is always predictable given the tree structure.

### `get_class_members()`

```cpp
void get_class_members(Idx x, std::vector<Idx>& out) const {
    validate_index(x);
    out.clear();
    Idx current = x;
    do {
        out.push_back(current);
        current = m_next[current];
    } while (current != x);
}
```

**Optimization**: Caller can `reserve(class_size(x))` before calling to eliminate reallocations.

### `validate_index()` (Private Helper)

```cpp
void validate_index(Idx x) const {
    if (x >= m_parent.size()) {
        throw std::runtime_error(
            "IterableUnionFind: index " + std::to_string(x) +
            " out of range [0, " + std::to_string(m_parent.size()) + ")");
    }
}
```

**Rationale**: As a standalone class that may be used by other code, we cannot rely on the caller to validate indices. Using `std::runtime_error` with a descriptive message allows programmers to identify the source of invalid use, unlike `vector.at()` which provides no context. Leaving indices unvalidated risks undefined behavior, which is unacceptable per project standards.

## Thread Safety

**Externally synchronized**: No internal synchronization. Caller must ensure:
- No concurrent modifications
- Concurrent const operations are safe only if no non-const operations occur

This matches `GraphCore`'s threading model.

## Test Plan

### Basic Operations

| Test | Description |
|------|-------------|
| `MakeSet_SingletonHasSizeOne` | New element has size 1 |
| `MakeSet_SingletonIsSelfRoot` | `find(x) == x` for new element |
| `MakeSet_SingletonCircularList` | `get_class_members(x)` returns only `{x}` |
| `MakeSet_SequentialIndices` | Indices assigned 0, 1, 2, ... |

### Unite Operations

| Test | Description |
|------|-------------|
| `Unite_TwoSingletons_MergesCorrectly` | Size becomes 2, both find same root |
| `Unite_SameClass_ReturnsFalse` | Idempotent, no state change |
| `Unite_SameClass_SizeUnchanged` | Size remains unchanged on no-op |
| `Unite_ChainOfThree_AllSameRoot` | A∪B, B∪C → all same root |
| `Unite_RankDecision_SmallerUnderLarger` | Union-by-rank respected |

### Size Tracking

| Test | Description |
|------|-------------|
| `Size_TotalityInvariant_AfterMakeSet` | Sum of sizes equals element count |
| `Size_TotalityInvariant_AfterUnite` | Invariant maintained after merges |
| `Size_NonRootReturnsCorrectSize` | `class_size(non_root)` works correctly |
| `Size_RootReturnsCorrectSize` | `class_size(root)` works correctly |

### Circular List Enumeration

| Test | Description |
|------|-------------|
| `GetMembers_Singleton_ReturnsSelf` | Single element list |
| `GetMembers_TwoElements_ReturnsBoth` | After unite, both present |
| `GetMembers_FromAnyMember_SameResult` | Can start enumeration from any member |
| `GetMembers_LargeClass_AllPresent` | Stress test with many elements |
| `GetMembers_CountMatchesSize` | `out.size() == class_size(x)` |

### Path Compression

| Test | Description |
|------|-------------|
| `Find_CompressesPath` | After find, parent points directly to root |
| `Find_DoesNotAffectCircularList` | Compression doesn't break iteration |
| `ClassRoot_DoesNotCompress` | Const version leaves tree unchanged |

### Index Validation

| Test | Description |
|------|-------------|
| `Find_InvalidIndex_Throws` | `find(999)` on empty throws `std::runtime_error` |
| `Unite_InvalidIndex_Throws` | `unite(0, 999)` throws with descriptive message |
| `ClassSize_InvalidIndex_Throws` | `class_size(999)` throws |
| `GetMembers_InvalidIndex_Throws` | `get_class_members(999, out)` throws |
| `ErrorMessage_ContainsIndex` | Exception message includes the invalid index |
| `ErrorMessage_ContainsRange` | Exception message includes valid range |

### Edge Cases

| Test | Description |
|------|-------------|
| `Empty_ElementCountZero` | Fresh instance has no elements |
| `Unite_WithSelf_ReturnsFalse` | `unite(x, x)` is no-op |

## Resolved Design Questions

### Q1: Recursive vs Iterative `find()`

**Decision**: Use iterative two-pass path compression.

- **Pass 1**: Traverse to find the root
- **Pass 2**: Traverse again to rewrite parent pointers to the root

This avoids stack overflow risks on deep trees and is explicit about what each pass accomplishes.

### Q2: Should `class_root()` also compress?

**Decision**: Keep `class_root()` const and non-compressing.

Callers needing performance should use the non-const `find()`. The const version exists for read-only contexts where modifying state is not permitted.

### Q3: Index Validation

**Decision**: Always validate indices and throw `std::runtime_error` with a descriptive message.

As a standalone class that may be used by other code, we cannot rely on the caller to validate indices. Rationale:
- `vector.at()` throws but with no context about the source
- No validation leads to undefined behavior, which is unacceptable per project standards (see CLAUDE.md)
- `std::runtime_error` with a message allows programmers to identify invalid use

### Q4: Circular List Splice Location

**Decision**: Splice at the roots, not the original arguments.

While splicing at any two elements from different lists is mathematically correct, using the roots provides deterministic behavior that aids debugging—the splice point is predictable given the tree structure.

---

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-01-08 | Claude Opus 4.5 | Initial design document extracted from eager cycle detection research |
| 2026-01-08 | Claude Opus 4.5 | Resolved Q1-Q4: iterative two-pass find(), const class_root(), mandatory index validation with std::runtime_error, splice at roots |

---

*This document is Active and open for editing by both AI agents and humans.*
