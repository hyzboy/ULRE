/// FeatureIntentValidation.cpp — Phase 5: Intent vs Effective Feature Mask Validation
///
/// Implementation of intent_features vs effective_feature_mask validation
/// to detect degradation/mismatch at Registry Acquire time.

#include <hgl/mtl/FeatureIntentValidation.h>
#include <hgl/mtl/MaterialFeature.h>
#include <sstream>
#include <iomanip>

namespace hgl::graph::mtl
{

FeatureIntentValidationResult ValidateFeatureIntentVsEffective(
    uint64_t intent_features,
    uint64_t effective_feature_mask,
    MaterialPreset preset)
{
    FeatureIntentValidationResult result;

    // If intent_features is 0, use preset default (always valid for backward compat)
    if (intent_features == 0)
    {
        result.is_valid = true;
        result.mismatch_reason = "intent_features not specified; using preset default";
        return result;
    }

    // intent_features != 0: Validate against effective mask
    // The effective mask should have been resolved from intent_features + preset mapping
    // Any mismatch indicates either:
    // 1. Degradation due to resource constraints
    // 2. Incompatible feature combinations that were revised
    // 3. Unsupported feature request that fell back to safe default

    // Define known feature bits for diagnostic purposes
    static const struct FeatureInfo
    {
        MaterialFeature feature;
        const char *name;
    } kFeatures[] =
    {
        { MaterialFeature::NeedsCamera,       "NeedsCamera" },
        { MaterialFeature::NeedsSky,          "NeedsSky" },
        { MaterialFeature::EnableLighting,    "EnableLighting" },
        { MaterialFeature::SurfaceStandard,   "SurfaceStandard" },
        { MaterialFeature::SurfaceUnlit,      "SurfaceUnlit" },
        { MaterialFeature::LightingLambert,   "LightingLambert" },
        { MaterialFeature::LightingBlinnPhong, "LightingBlinnPhong" },
        { MaterialFeature::LightingPBR,       "LightingPBR" },
    };

    // Detect mismatches by comparing each known feature bit
    static const size_t kFeatureCount = 8;
    bool has_mismatch = false;
    for (size_t i = 0; i < kFeatureCount; ++i)
    {
        const auto &fi = kFeatures[i];
    
        uint64_t feature_mask = ToFeatureMask(fi.feature);
        bool in_intent = (intent_features & feature_mask) != 0;
        bool in_effective = (effective_feature_mask & feature_mask) != 0;

        if (in_intent != in_effective)
        {
            has_mismatch = true;
            result.mismatched_features.push_back(
                std::string(fi.name) + (in_intent ? " [intent:yes→effective:no]" : " [intent:no→effective:yes]")
            );
        }
    }

    if (has_mismatch)
    {
        result.is_valid = false;
        
        std::ostringstream oss;
        oss << "Feature mismatch detected: intent_features(0x"
            << std::hex << std::setfill('0') << std::setw(016) << intent_features
            << ") != effective_feature_mask(0x"
            << std::setfill('0') << std::setw(016) << effective_feature_mask
            << std::dec << ")";
        result.mismatch_reason = oss.str();
    }
    else
    {
        result.is_valid = true;
        result.mismatch_reason = "intent_features matches effective_feature_mask";
    }

    return result;
}

std::string BuildFeatureIntentMismatchMessage(
    const FeatureIntentValidationResult &validation,
    const std::string &material_name,
    uint64_t intent_features,
    uint64_t effective_feature_mask)
{
    std::ostringstream oss;

    oss << "[MaterialRecipeRegistry] Feature Intent Mismatch\n";
    oss << "  Material: " << material_name << "\n";
    oss << "  Intent Features:         0x" << std::hex << std::setfill('0') << std::setw(016) << intent_features << "\n";
    oss << "  Effective Feature Mask:  0x" << std::setfill('0') << std::setw(016) << effective_feature_mask << "\n";
    oss << "  Status: " << (validation.is_valid ? "VALID" : "MISMATCH") << "\n";

    if (!validation.mismatch_reason.empty())
    {
        oss << "  Reason: " << validation.mismatch_reason << "\n";
    }

    if (!validation.mismatched_features.empty())
    {
        oss << "  Mismatched Features:\n";
        for (const auto &mf : validation.mismatched_features)
        {
            oss << "    - " << mf << "\n";
        }
    }

    return oss.str();
}

} // namespace hgl::graph::mtl
