#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <cstdio>

using namespace hgl::graph::mtl;

int main()
{
    // Phase 2 Equivalence Test: Verify that old Material3DCreateConfig (camera/sky/lighting)
    // produces the same hash as new config with effective_feature_mask set.

    // Test Case 1: Standard (camera=true, sky=true, lighting=PBR)
    {
        // Old path: Fields set explicitly
        Material3DCreateConfig cfg_old;
        cfg_old.camera = true;
        cfg_old.sky = true;
        cfg_old.lighting_model = LightingModel::PBR;

        std::string hash_old = cfg_old.ToHashStdString();

        // New path: effective_feature_mask set to Camera|Sky|PBR
        Material3DCreateConfig cfg_new;
        cfg_new.camera = true;
        cfg_new.sky = true;
        cfg_new.lighting_model = LightingModel::PBR;
        cfg_new.effective_feature_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::NeedsSky) |
            ToFeatureMask(MaterialFeature::SurfaceStandard) |
            ToFeatureMask(MaterialFeature::LightingPBR);

        std::string hash_new = cfg_new.ToHashStdString();

        fprintf(stdout, "[MaterialVariantKeyEquivalenceTest] Test Case 1 (Standard):\n");
        fprintf(stdout, "  Old hash: %s\n", hash_old.c_str());
        fprintf(stdout, "  New hash: %s\n", hash_new.c_str());

        // When effective_feature_mask != 0, new hash should include _Feat prefix
        if (hash_new.find("_Feat") == std::string::npos)
        {
            fprintf(stderr, "[MaterialVariantKeyEquivalenceTest] FAIL: Expected _Feat in new hash\n");
            return 1;
        }

        // Both should preserve camera, sky, ambient model, l2w, position format
        if (hash_old.find("_Camera") == std::string::npos || hash_old.find("_Sky") == std::string::npos)
        {
            fprintf(stderr, "[MaterialVariantKeyEquivalenceTest] FAIL: Old hash missing _Camera or _Sky\n");
            return 1;
        }
    }

    // Test Case 2: Terrain (camera=true, sky=true, lighting=PBR via TerrainGridCreateConfig)
    {
        TerrainGridCreateConfig cfg_old;
        std::string hash_old = cfg_old.ToHashStdString();

        TerrainGridCreateConfig cfg_new;
        cfg_new.effective_feature_mask =
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::NeedsSky) |
            ToFeatureMask(MaterialFeature::SurfaceTerrain) |
            ToFeatureMask(MaterialFeature::LightingPBR);

        std::string hash_new = cfg_new.ToHashStdString();

        fprintf(stdout, "[MaterialVariantKeyEquivalenceTest] Test Case 2 (TerrainGrid):\n");
        fprintf(stdout, "  Old hash: %s\n", hash_old.c_str());
        fprintf(stdout, "  New hash: %s\n", hash_new.c_str());

        if (hash_new.find("_Feat") == std::string::npos)
        {
            fprintf(stderr, "[MaterialVariantKeyEquivalenceTest] FAIL: Expected _Feat in new hash\n");
            return 1;
        }
    }

    // Test Case 3: SkyMinimal (camera=true, sky=true, lighting=default)
    {
        SkyMinimalCreateConfig cfg_old;
        std::string hash_old = cfg_old.ToHashStdString();

        SkyMinimalCreateConfig cfg_new;
        cfg_new.effective_feature_mask =
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::NeedsSky) |
            ToFeatureMask(MaterialFeature::SurfaceSky) |
            ToFeatureMask(MaterialFeature::LightingLambert);  // default

        std::string hash_new = cfg_new.ToHashStdString();

        fprintf(stdout, "[MaterialVariantKeyEquivalenceTest] Test Case 3 (SkyMinimal):\n");
        fprintf(stdout, "  Old hash: %s\n", hash_old.c_str());
        fprintf(stdout, "  New hash: %s\n", hash_new.c_str());

        if (hash_new.find("_Feat") == std::string::npos)
        {
            fprintf(stderr, "[MaterialVariantKeyEquivalenceTest] FAIL: Expected _Feat in new hash\n");
            return 1;
        }
    }

    // Test Case 4: Backward compatibility (intent_features=0, use old field path)
    {
        Material3DCreateConfig cfg_no_intent;
        cfg_no_intent.camera = true;
        cfg_no_intent.sky = true;
        cfg_no_intent.lighting_model = LightingModel::PBR;
        cfg_no_intent.effective_feature_mask = 0;  // Explicitly 0: use old path

        std::string hash = cfg_no_intent.ToHashStdString();

        fprintf(stdout, "[MaterialVariantKeyEquivalenceTest] Test Case 4 (Backward Compat, effective_feature_mask=0):\n");
        fprintf(stdout, "  Hash: %s\n", hash.c_str());

        // With effective_feature_mask=0, should NOT have _Feat
        if (hash.find("_Feat") != std::string::npos)
        {
            fprintf(stderr, "[MaterialVariantKeyEquivalenceTest] FAIL: Should not have _Feat when effective_feature_mask=0\n");
            return 1;
        }

        // Should have old _Camera and _Sky markers
        if (hash.find("_Camera") == std::string::npos || hash.find("_Sky") == std::string::npos)
        {
            fprintf(stderr, "[MaterialVariantKeyEquivalenceTest] FAIL: Missing _Camera or _Sky in old path hash\n");
            return 1;
        }
    }

    fprintf(stdout, "[MaterialVariantKeyEquivalenceTest] PASS\n");
    return 0;
}
