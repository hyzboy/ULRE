/// FeatureIntentValidation.h — Phase 5: Intent vs Effective Feature Mask Validation
///
/// Provides validation routines to detect mismatches between:
/// - intent_features: Author-specified feature requirements in MaterialRecipe
/// - effective_feature_mask: Runtime-resolved feature mask used for caching/binding
///
/// Validation modes:
/// - Strict (strict_intent_features=true): Fails on mismatch, logs diagnostic
/// - Non-strict (strict_intent_features=false): Warns on mismatch, allows degradation

#pragma once

#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl
{

/// Validation result for intent vs effective feature mismatch
struct FeatureIntentValidationResult
{
    bool                is_valid = true;           ///< true = intent matches effective
    std::string         mismatch_reason;           ///< Human-readable description
    std::vector<std::string> mismatched_features;  ///< List of features that differ

    explicit operator bool() const { return is_valid; }
};

/// Validates that intent_features and effective_feature_mask are consistent
/// 
/// - If intent_features == 0: Uses preset default mapping (always valid)
/// - If intent_features != 0: Compares against actual effective feature mask
/// 
/// Returns detailed result with mismatch information for diagnostics
FeatureIntentValidationResult ValidateFeatureIntentVsEffective(
    uint64_t intent_features,           // From MaterialRecipe::intent_features
    uint64_t effective_feature_mask,    // Actual runtime-resolved mask
    MaterialPreset preset               // Used for default mapping context
);

/// Builds human-readable diagnostic message for feature intent mismatch
std::string BuildFeatureIntentMismatchMessage(
    const FeatureIntentValidationResult &validation,
    const std::string &material_name,
    uint64_t intent_features,
    uint64_t effective_feature_mask
);

} // namespace hgl::graph::mtl
