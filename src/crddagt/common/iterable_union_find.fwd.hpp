/**
 * @file iterable_union_find.fwd.hpp
 * @brief Forward declaration of the IterableUnionFind class template.
 */
#pragma once

namespace crddagt {

/**
 * @brief Forward declaration of IterableUnionFind.
 *
 * @tparam Idx The index type, defaults to size_t. Must be unsigned.
 *         Supports uint16_t, uint32_t, uint64_t, or size_t.
 */
template <typename Idx>
class IterableUnionFind;

} // namespace crddagt
