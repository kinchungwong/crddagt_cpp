# 2026-01-08_185125_eager_cycle_detection_snippets.md

- Author: Claude Opus 4.5 (Anthropic)
- Date: 2026-01-08 (America/Los_Angeles)
- Document status: Active

## See Also

- [2026-01-08_180853_eager_cycle_detection_research.md](2026-01-08_180853_eager_cycle_detection_research.md) - High-level research document (references these snippets)
- [2026-01-08_202059_iterable_union_find_design.md](2026-01-08_202059_iterable_union_find_design.md) - Standalone IterableUnionFind class design

## Purpose

This document contains sample code snippets for implementing eager cycle detection in `GraphCore`. These snippets are separated from the research document to keep that document at a high abstraction level.

**Note**: These are illustrative sketches, not production-ready code. The actual implementation may differ.

---

## Option A: Simple DFS Reachability

```cpp
bool can_reach(StepIdx from, StepIdx target, const AdjacencyList& graph) {
    std::vector<bool> visited(step_count, false);
    std::stack<StepIdx> stack;
    stack.push(from);

    while (!stack.empty()) {
        StepIdx current = stack.top();
        stack.pop();

        if (current == target) return true;
        if (visited[current]) continue;
        visited[current] = true;

        for (StepIdx neighbor : graph[current]) {
            if (!visited[neighbor]) {
                stack.push(neighbor);
            }
        }
    }
    return false;
}
```

**Complexity**: O(V+E) worst case, but typically much better due to early termination.

---

## Option B: Bidirectional BFS (Skeleton)

```cpp
bool would_create_cycle_bidirectional(StepIdx before, StepIdx after) {
    // BFS forward from 'after', BFS backward from 'before'
    // Stop when they meet or frontiers exhaust
}
```

**Complexity**: O(√(V+E)) average case for random graphs, but graph structure may vary.

---

## Option C: Incremental Transitive Closure (Concept)

```cpp
// After adding edge before→after:
// For all x that can reach 'before': x can now reach everything 'after' can reach
```

**Complexity**: O(V²) space, O(V²) per update worst case. Not recommended for large graphs.

---

## Data Structure: Forward Adjacency List

```cpp
// In GraphCore private members:
std::vector<std::vector<StepIdx>> m_step_successors;
// m_step_successors[s] = list of steps that s has edges to
```

Updated on:
- `link_steps()`: Add `after` to `m_step_successors[before]`
- `link_fields()`: Add induced implicit edges

---

## Modified `link_steps()`

```cpp
void GraphCore::link_steps(size_t before, size_t after, TrustLevel trust) {
    // Existing validation...

    if (before == after) {
        throw GraphCoreError(GraphCoreErrorCode::CycleDetected,
                             "Self-loop: step " + std::to_string(before));
    }

    if (m_eager_validation) {
        // Check if adding before→after creates a cycle
        // i.e., check if 'before' is reachable from 'after'
        if (is_reachable_from(after, before)) {
            throw GraphCoreError(GraphCoreErrorCode::CycleDetected,
                                 "Cycle detected: adding edge " +
                                 std::to_string(before) + "→" +
                                 std::to_string(after) +
                                 " creates a cycle");
        }
    }

    // Add the edge
    m_explicit_step_links.emplace_back(before, after);
    m_explicit_step_link_trust.push_back(trust);
    m_step_successors[before].push_back(after);
}
```

---

## Modified `link_fields()`

**Note**: Per AD3, all cross-class field pairs must be checked for induced dependencies.

```cpp
void GraphCore::link_fields(size_t field_a, size_t field_b, TrustLevel trust) {
    // Existing validation (type check)...

    FieldIdx root_a = uf_find(field_a);
    FieldIdx root_b = uf_find(field_b);

    // If already in same equivalence class, nothing to merge
    if (root_a == root_b) {
        return;
    }

    if (m_eager_validation) {
        // Collect all fields in each equivalence class
        // Pre-allocate using exact size tracked in union-find
        std::vector<FieldIdx> class_a;
        std::vector<FieldIdx> class_b;
        class_a.reserve(m_field_uf_size[root_a]);
        class_b.reserve(m_field_uf_size[root_b]);
        get_equivalence_class(root_a, class_a);
        get_equivalence_class(root_b, class_b);

        // Check all cross-class pairs for induced implicit edges
        std::vector<std::pair<StepIdx, StepIdx>> new_edges;

        for (FieldIdx fa : class_a) {
            for (FieldIdx fb : class_b) {
                StepIdx step_a = m_field_owner_step[fa];
                StepIdx step_b = m_field_owner_step[fb];

                if (step_a == step_b) continue;  // Same step, no edge

                auto edge = get_implicit_edge(step_a, m_field_usages[fa],
                                               step_b, m_field_usages[fb]);
                if (edge.has_value()) {
                    auto [before, after] = edge.value();

                    // Check if this creates a cycle
                    if (is_reachable_from(after, before)) {
                        throw GraphCoreError(GraphCoreErrorCode::CycleDetected,
                                             "Cycle detected: merging field equivalence classes "
                                             "induces edge that creates a cycle");
                    }

                    new_edges.push_back(edge.value());
                }
            }
        }

        // All checks passed; add the new implicit edges
        for (auto [before, after] : new_edges) {
            m_step_successors[before].push_back(after);
        }
    }

    // Union-find merge
    uf_unite(field_a, field_b);
    m_field_links.emplace_back(field_a, field_b);
    m_field_link_trust.push_back(trust);
}
```

---

## Helper: `get_implicit_edge()`

**Precondition**: UsageConstraintViolation checks (multiple Create, multiple Destroy) have already been performed. This helper is only called on field pairs known to be valid; Create-Create and Destroy-Destroy pairs are caught earlier.

```cpp
std::optional<std::pair<StepIdx, StepIdx>>
GraphCore::get_implicit_edge(StepIdx step_a, Usage usage_a,
                              StepIdx step_b, Usage usage_b) {
    // Precondition: usage_a and usage_b are not both Create, nor both Destroy
    // Usage ordering: Create < Read < Destroy
    int order_a = static_cast<int>(usage_a);
    int order_b = static_cast<int>(usage_b);

    if (order_a < order_b) {
        return std::make_pair(step_a, step_b);  // step_a → step_b
    } else if (order_b < order_a) {
        return std::make_pair(step_b, step_a);  // step_b → step_a
    } else {
        // Same usage (Read-Read only, given precondition)
        return std::nullopt;
    }
}
```

---

## Equivalence Class Member Tracking

To achieve O(class_size) enumeration instead of O(n) scanning, maintain a circular linked list of equivalence class members using a vector-backed next-pointer array.

### Data Structure

```cpp
// In GraphCore private members (in addition to existing union-find arrays):
std::vector<FieldIdx> m_field_uf_next;
// m_field_uf_next[i] = next field in the same equivalence class (circular)
// For a singleton: m_field_uf_next[i] = i

std::vector<size_t> m_field_uf_size;
// m_field_uf_size[i] = size of equivalence class if i is root, else 0
// For a singleton: m_field_uf_size[i] = 1
// Invariant: sum of all m_field_uf_size[i] == m_field_count
```

### Functions Requiring Changes in GraphCore

| Function | Change Required |
|----------|-----------------|
| `add_field()` | Initialize `m_field_uf_next[field_idx] = field_idx` and `m_field_uf_size[field_idx] = 1` |
| `uf_unite()` | Splice circular lists; update size: new root gets sum, old root gets 0 |
| `get_equivalence_class()` | Iterate the circular list instead of scanning all fields |

### Modified `uf_unite()`

```cpp
void GraphCore::uf_unite(FieldIdx a, FieldIdx b) {
    FieldIdx root_a = uf_find(a);
    FieldIdx root_b = uf_find(b);

    if (root_a == root_b) return;  // Already in same class

    // Compute combined size before modifying roots
    size_t combined_size = m_field_uf_size[root_a] + m_field_uf_size[root_b];

    // Union by rank (existing logic)
    FieldIdx new_root;
    FieldIdx old_root;
    if (m_field_uf_rank[root_a] < m_field_uf_rank[root_b]) {
        m_field_uf_parent[root_a] = root_b;
        new_root = root_b;
        old_root = root_a;
    } else if (m_field_uf_rank[root_a] > m_field_uf_rank[root_b]) {
        m_field_uf_parent[root_b] = root_a;
        new_root = root_a;
        old_root = root_b;
    } else {
        m_field_uf_parent[root_b] = root_a;
        m_field_uf_rank[root_a]++;
        new_root = root_a;
        old_root = root_b;
    }

    // Update sizes: new root gets combined size, old root gets 0
    m_field_uf_size[new_root] = combined_size;
    m_field_uf_size[old_root] = 0;

    // Splice the circular lists at the roots for deterministic behavior
    // Before: root_a → ... → root_a  and  root_b → ... → root_b
    // After:  root_a → root_b.next → ... → root_b → root_a.next → ... → root_a
    std::swap(m_field_uf_next[root_a], m_field_uf_next[root_b]);
}
```

### Helper: `get_equivalence_class()`

```cpp
void GraphCore::get_equivalence_class(FieldIdx field, std::vector<FieldIdx>& out) const {
    out.clear();
    FieldIdx current = field;
    do {
        out.push_back(current);
        current = m_field_uf_next[current];
    } while (current != field);
}
```

**Complexity**: O(class_size) instead of O(n).

**Note**: The caller (e.g., `link_fields()`) can pre-allocate the output vector with `reserve(m_field_uf_size[root])` to eliminate reallocation overhead entirely, since exact class sizes are tracked.

---

## Helper: `is_reachable_from()`

```cpp
bool GraphCore::is_reachable_from(StepIdx from, StepIdx target) const {
    if (from == target) return true;

    std::vector<bool> visited(m_step_count, false);
    std::stack<StepIdx> stack;
    stack.push(from);

    while (!stack.empty()) {
        StepIdx current = stack.top();
        stack.pop();

        if (visited[current]) continue;
        visited[current] = true;

        for (StepIdx successor : m_step_successors[current]) {
            if (successor == target) return true;
            if (!visited[successor]) {
                stack.push(successor);
            }
        }
    }
    return false;
}
```

---

## Change Log

| Date | Author | Change |
|------|--------|--------|
| 2026-01-08 | Claude Opus 4.5 | Initial extraction from research document |
| 2026-01-08 | Claude Opus 4.5 | Updated link_fields() to check all cross-class pairs per AD3; added get_equivalence_class() helper |
| 2026-01-08 | Claude Opus 4.5 | Added circular linked list data structure for O(class_size) enumeration; updated uf_unite() and get_equivalence_class() |
| 2026-01-08 | Claude Opus 4.5 | Changed get_equivalence_class() to use user-allocated output vector; added note about reserve() pre-allocation |
| 2026-01-08 | Claude Opus 4.5 | Added m_field_uf_size for exact class size tracking; updated uf_unite() to maintain sizes; totality invariant documented |
| 2026-01-08 | Claude Opus 4.5 | Added precondition to get_implicit_edge() clarifying UsageConstraintViolation checks occur first |
| 2026-01-08 | Claude Opus 4.5 | Changed uf_unite() to splice circular lists at roots (not original arguments) for deterministic debugging |

---

*This document is Active and open for editing by both AI agents and humans.*
