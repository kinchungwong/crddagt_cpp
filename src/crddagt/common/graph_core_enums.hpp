/**
 * @file graph_core_enums.hpp
 */
#pragma once
#include "crddagt/common/common.hpp"

namespace crddagt
{

// ============================================================================
// Index type aliases
// ============================================================================

/**
 * @brief Type alias for step indices.
 *
 * @details
 * `StepIdx` is a type alias for `size_t` used to identify steps in the graph.
 * This alias exists for clarity in API signatures and documentation, not for
 * compile-time type safety.
 */
using StepIdx = size_t;

/**
 * @brief Type alias for field indices.
 *
 * @details
 * `FieldIdx` is a type alias for `size_t` used to identify fields in the graph.
 * This alias exists for clarity in API signatures and documentation, not for
 * compile-time type safety.
 */
using FieldIdx = size_t;

/**
 * @brief Type alias for data object indices.
 *
 * @details
 * `DataIdx` is a type alias for `size_t` used to identify data objects in the
 * exported graph. Fields that are linked together share the same `DataIdx`.
 * This alias exists for clarity in API signatures and documentation, not for
 * compile-time type safety.
 */
using DataIdx = size_t;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Enumeration of field usage types.
 *
 * @details
 * The field usage types induce an execution order dependency between steps that
 * use the same data via fields. There has to be exactly one Create, which can be
 * added at any time, but attempts to add a second one will fail. There can be any
 * number of Reads, and their steps will be automatically scheduled after the
 * step that Creates it. There can be zero or one Destroy, which if present will
 * be scheduled after all Reads.
 */
enum class Usage
{
    Create,
    Read,
    Destroy
};

/**
 * @brief Enumeration of trust levels for links.
 *
 * @details
 * Links (both step-to-step and field-to-field) can be assigned trust levels to
 * indicate confidence in their correctness. When graph diagnostics detect issues
 * such as cycles or constraint violations, lower-trust links are more likely to
 * be identified as the source of the problem.
 *
 * @par Blame priority
 * - `Low`: Most likely to be blamed when issues are detected.
 * - `Middle`: Moderate confidence; blamed if no low-trust links are involved.
 * - `High`: Least likely to be blamed; assumed correct unless no alternative.
 *
 * @par Use cases
 * - User-specified links may be assigned `Low` trust (user input is error-prone).
 * - Framework-inferred links may be assigned `High` trust (derived from reliable sources).
 * - Default or heuristic links may use `Middle` trust.
 */
enum class TrustLevel
{
    Low,
    Middle,
    High
};

} // namespace crddagt
