/**
 * @file graph_core_diagnostics.hpp
 */
#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/graph_core_enums.hpp"

namespace crddagt
{

// ============================================================================
// Diagnostic item types
// ============================================================================

/**
 * @brief Severity level for diagnostic items.
 */
enum class DiagnosticSeverity
{
    Warning,  ///< Non-blocking issue that may indicate a problem.
    Error     ///< Blocking issue that prevents graph export.
};

/**
 * @brief Category of diagnostic issue.
 */
enum class DiagnosticCategory
{
    Cycle,                  ///< A cycle was detected in the step ordering.
    MultipleCreate,         ///< More than one Create field for the same data.
    MultipleDestroy,        ///< More than one Destroy field for the same data.
    UnsafeSelfAliasing,     ///< Same step has incompatible usages for same data.
    MissingCreate,          ///< Read/Destroy field without a corresponding Create.
    TypeMismatch,           ///< Linked fields have incompatible types.
    OrphanStep,             ///< A step has no fields or links.
    UnusedData,             ///< Create field with no Read or Destroy consumers.
    InternalError           ///< An internal consistency error.
};

/**
 * @brief A single diagnostic item (error or warning).
 *
 * @details
 * Each diagnostic item describes one issue detected during graph validation.
 * The `blamed_links` field contains indices into the link arrays, ordered by
 * suspicion (lower trust = more suspicious).
 */
struct DiagnosticItem
{
    DiagnosticSeverity severity;
    DiagnosticCategory category;
    std::string message;

    /// Step indices involved in this issue (if applicable).
    std::vector<StepIdx> involved_steps;

    /// Field indices involved in this issue (if applicable).
    std::vector<FieldIdx> involved_fields;

    /// Indices of blamed step links (into GraphCore's explicit step links).
    /// Ordered by suspicion: lower-trust links appear first.
    std::vector<size_t> blamed_step_links;

    /// Indices of blamed field links (into GraphCore's field links).
    /// Ordered by suspicion: lower-trust links appear first.
    std::vector<size_t> blamed_field_links;
};

// ============================================================================
// GraphCoreDiagnostics
// ============================================================================

/**
 * @brief Diagnostic information collected from a GraphCore instance.
 *
 * @details
 * `GraphCoreDiagnostics` contains all errors and warnings detected during
 * graph construction or validation. It is produced by `GraphCore::get_diagnostics()`.
 *
 * @par Error vs Warning
 * - **Errors** are blocking issues that prevent the graph from being exported.
 *   Examples: Cycle, MultipleCreate, MultipleDestroy, UnsafeSelfAliasing,
 *   TypeMismatch. Also MissingCreate when the graph is considered sealed.
 * - **Warnings** are non-blocking issues that may indicate problems but do not
 *   prevent export. Examples: OrphanStep, UnusedData.
 *   Also MissingCreate when the graph is not considered sealed.
 *
 * @par Seal-sensitivity
 * The `MissingCreate` diagnostic is the only seal-sensitive diagnostic.
 * - When `get_diagnostics(false)` is called (default), MissingCreate is a Warning.
 * - When `get_diagnostics(true)` is called (sealed), MissingCreate is an Error.
 * All other diagnostics have fixed severity regardless of the seal parameter.
 *
 * @par Blame analysis
 * When issues are detected, the diagnostic system attempts to identify which
 * links are most likely responsible. Links with lower `TrustLevel` are blamed
 * first. The `blamed_step_links` and `blamed_field_links` fields in each
 * `DiagnosticItem` contain indices into the corresponding link arrays.
 *
 * @par Thread safety
 * - No internal synchronization.
 * - Once constructed, the data is immutable.
 * - Concurrent reads are safe.
 */
class GraphCoreDiagnostics
{
public:
    /**
     * @brief Check if the graph has any errors.
     * @return True if there are errors that prevent export.
     */
    bool has_errors() const noexcept
    {
        return !m_errors.empty();
    }

    /**
     * @brief Check if the graph has any warnings.
     * @return True if there are warnings (non-blocking issues).
     */
    bool has_warnings() const noexcept
    {
        return !m_warnings.empty();
    }

    /**
     * @brief Check if the graph is valid for export.
     * @return True if there are no errors (warnings are allowed).
     */
    bool is_valid() const noexcept
    {
        return m_errors.empty();
    }

    /**
     * @brief Get all error items.
     * @return Reference to the vector of error items.
     */
    const std::vector<DiagnosticItem>& errors() const noexcept
    {
        return m_errors;
    }

    /**
     * @brief Get all warning items.
     * @return Reference to the vector of warning items.
     */
    const std::vector<DiagnosticItem>& warnings() const noexcept
    {
        return m_warnings;
    }

    /**
     * @brief Get all diagnostic items (errors and warnings combined).
     * @return A vector containing all items, errors first then warnings.
     */
    std::vector<DiagnosticItem> all_items() const
    {
        std::vector<DiagnosticItem> result;
        result.reserve(m_errors.size() + m_warnings.size());
        result.insert(result.end(), m_errors.begin(), m_errors.end());
        result.insert(result.end(), m_warnings.begin(), m_warnings.end());
        return result;
    }

    // Allow GraphCore to populate diagnostics
    friend class GraphCore;

private:
    std::vector<DiagnosticItem> m_errors;
    std::vector<DiagnosticItem> m_warnings;
};

} // namespace crddagt
