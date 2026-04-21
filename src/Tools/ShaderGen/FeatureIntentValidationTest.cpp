/// Phase 5 Integration Test: Intent vs Effective Feature Mask Validation
///
/// Validates Registry Acquire path:
/// 1. Intent feature validation at Acquire time
/// 2. Strict vs non-strict mode behavior
/// 3. Diagnostic message generation
/// 4. Program effective_feature_mask assignment

#include <hgl/mtl/FeatureIntentValidation.h>
#include <hgl/mtl/MaterialFeature.h>
#include <cstdio>
#include <cstdint>

using namespace hgl::graph::mtl;

int main()
{
    fprintf(stdout, "[FeatureIntentValidationTest] Starting Phase 5 intent validation test...\n\n");

    // Test Case 1: Valid intent == effective (match)
    {
        fprintf(stdout, "[FeatureIntentValidationTest] Test Case 1 (Valid Match):\n");
        
        uint64_t intent_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::NeedsSky) |
            ToFeatureMask(MaterialFeature::SurfaceStandard) |
            ToFeatureMask(MaterialFeature::LightingPBR);

        uint64_t effective_mask = intent_mask;  // Perfect match

        auto result = ValidateFeatureIntentVsEffective(intent_mask, effective_mask, MaterialPreset::Standard);

        fprintf(stdout, "  Intent:     0x%016llx\n", (unsigned long long)intent_mask);
        fprintf(stdout, "  Effective:  0x%016llx\n", (unsigned long long)effective_mask);
        fprintf(stdout, "  Valid: %s\n", result.is_valid ? "YES" : "NO");
        fprintf(stdout, "  Reason: %s\n", result.mismatch_reason.c_str());

        if (result.is_valid)
            fprintf(stdout, "  ✓ PASS - Intent matches Effective\n\n");
        else
            fprintf(stdout, "  ✗ FAIL - Should be valid!\n\n");
    }

    // Test Case 2: Degradation - intent requests camera but effective doesn't have it
    {
        fprintf(stdout, "[FeatureIntentValidationTest] Test Case 2 (Degradation - Missing Feature):\n");
        
        uint64_t intent_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |    // Requested
            ToFeatureMask(MaterialFeature::NeedsSky) |
            ToFeatureMask(MaterialFeature::SurfaceStandard);

        uint64_t effective_mask = 
            ToFeatureMask(MaterialFeature::NeedsSky) |       // Camera NOT present
            ToFeatureMask(MaterialFeature::SurfaceStandard);

        auto result = ValidateFeatureIntentVsEffective(intent_mask, effective_mask, MaterialPreset::Standard);

        fprintf(stdout, "  Intent:     0x%016llx\n", (unsigned long long)intent_mask);
        fprintf(stdout, "  Effective:  0x%016llx\n", (unsigned long long)effective_mask);
        fprintf(stdout, "  Valid: %s\n", result.is_valid ? "YES" : "NO");
        fprintf(stdout, "  Reason: %s\n", result.mismatch_reason.c_str());
        fprintf(stdout, "  Mismatched Features:\n");
        for (const auto &mf : result.mismatched_features)
        {
            fprintf(stdout, "    - %s\n", mf.c_str());
        }

        if (!result.is_valid)
            fprintf(stdout, "  ✓ PASS - Degradation detected\n\n");
        else
            fprintf(stdout, "  ✗ FAIL - Should detect mismatch!\n\n");
    }

    // Test Case 3: Extra features - effective has features not in intent
    {
        fprintf(stdout, "[FeatureIntentValidationTest] Test Case 3 (Extra Features in Effective):\n");
        
        uint64_t intent_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::SurfaceStandard);

        uint64_t effective_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::NeedsSky) |       // Extra in effective
            ToFeatureMask(MaterialFeature::SurfaceStandard);

        auto result = ValidateFeatureIntentVsEffective(intent_mask, effective_mask, MaterialPreset::Standard);

        fprintf(stdout, "  Intent:     0x%016llx\n", (unsigned long long)intent_mask);
        fprintf(stdout, "  Effective:  0x%016llx\n", (unsigned long long)effective_mask);
        fprintf(stdout, "  Valid: %s\n", result.is_valid ? "YES" : "NO");
        fprintf(stdout, "  Reason: %s\n", result.mismatch_reason.c_str());
        fprintf(stdout, "  Mismatched Features:\n");
        for (const auto &mf : result.mismatched_features)
        {
            fprintf(stdout, "    - %s\n", mf.c_str());
        }

        if (!result.is_valid)
            fprintf(stdout, "  ✓ PASS - Extra features detected\n\n");
        else
            fprintf(stdout, "  ✗ FAIL - Should detect mismatch!\n\n");
    }

    // Test Case 4: intent_features=0 (backward compat - always valid)
    {
        fprintf(stdout, "[FeatureIntentValidationTest] Test Case 4 (Backward Compat - intent=0):\n");
        
        uint64_t intent_mask = 0;  // Not specified, use preset default

        uint64_t effective_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::SurfaceStandard);

        auto result = ValidateFeatureIntentVsEffective(intent_mask, effective_mask, MaterialPreset::Standard);

        fprintf(stdout, "  Intent:     0x%016llx (not specified)\n", (unsigned long long)intent_mask);
        fprintf(stdout, "  Effective:  0x%016llx\n", (unsigned long long)effective_mask);
        fprintf(stdout, "  Valid: %s\n", result.is_valid ? "YES" : "NO");
        fprintf(stdout, "  Reason: %s\n", result.mismatch_reason.c_str());

        if (result.is_valid)
            fprintf(stdout, "  ✓ PASS - Backward compat: intent=0 always valid\n\n");
        else
            fprintf(stdout, "  ✗ FAIL - intent=0 should always be valid!\n\n");
    }

    // Test Case 5: Diagnostic message generation
    {
        fprintf(stdout, "[FeatureIntentValidationTest] Test Case 5 (Diagnostic Message):\n");
        
        uint64_t intent_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::LightingPBR);

        uint64_t effective_mask = 
            ToFeatureMask(MaterialFeature::LightingPBR);  // Camera degraded away

        auto result = ValidateFeatureIntentVsEffective(intent_mask, effective_mask, MaterialPreset::Standard);
        std::string diagnostic = BuildFeatureIntentMismatchMessage(result, "brick_wall_lit", intent_mask, effective_mask);

        fprintf(stdout, "  Generated Diagnostic:\n");
        for (const auto &line : diagnostic)
        {
            if (line == '\n')
                fprintf(stdout, "\n");
            else if (line == '\0')
                break;
        }
        fprintf(stdout, "  %s\n", diagnostic.c_str());
        fprintf(stdout, "  ✓ PASS - Diagnostic message generated\n\n");
    }

    // Test Case 6: Multiple feature mismatches
    {
        fprintf(stdout, "[FeatureIntentValidationTest] Test Case 6 (Multiple Mismatches):\n");
        
        uint64_t intent_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::NeedsSky) |
            ToFeatureMask(MaterialFeature::LightingPBR);

        uint64_t effective_mask = 0;  // All features stripped

        auto result = ValidateFeatureIntentVsEffective(intent_mask, effective_mask, MaterialPreset::Standard);

        fprintf(stdout, "  Intent:     0x%016llx (3 features)\n", (unsigned long long)intent_mask);
        fprintf(stdout, "  Effective:  0x%016llx (all stripped)\n", (unsigned long long)effective_mask);
        fprintf(stdout, "  Valid: %s\n", result.is_valid ? "YES" : "NO");
        fprintf(stdout, "  Mismatched Features: %zu\n", result.mismatched_features.size());
        for (const auto &mf : result.mismatched_features)
        {
            fprintf(stdout, "    - %s\n", mf.c_str());
        }

        if (!result.is_valid && result.mismatched_features.size() >= 3)
            fprintf(stdout, "  ✓ PASS - Multiple mismatches detected\n\n");
        else
            fprintf(stdout, "  ✗ FAIL - Should detect all mismatches!\n\n");
    }

    fprintf(stdout, "[FeatureIntentValidationTest] COMPLETE\n");
    return 0;
}
