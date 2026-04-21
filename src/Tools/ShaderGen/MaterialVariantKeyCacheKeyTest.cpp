#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/LightingModel.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/type/FNV1a.h>
#include <cstdio>
#include <unordered_map>
#include <cstdint>

using namespace hgl::graph;
using namespace hgl::graph::mtl;

int main()
{
    // Phase 3 Cache Key Consistency Test:
    // Verify that MaterialVariantKey::Hash() distinguishes between
    // configs with and without effective_feature_mask.

    fprintf(stdout, "[MaterialVariantKeyCacheKeyTest] Starting Phase 3 cache key verification...\n");

    // Test Case 1: Two keys with same base fields but different effective_feature_mask
    {
        MaterialVariantKey key1, key2;
        
        // Both start with identical base configuration
        key1.surface_type = SurfaceType::Standard;
        key1.lighting_model = LightingModel::PBR;
        key1.sky_ambient_model = SkyLightAmbientModel::Simple;
        
        key2.surface_type = SurfaceType::Standard;
        key2.lighting_model = LightingModel::PBR;
        key2.sky_ambient_model = SkyLightAmbientModel::Simple;
        
        // key1: old path (no effective_feature_mask)
        key1.effective_feature_mask = 0;
        
        // key2: new path (with effective_feature_mask)
        key2.effective_feature_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::NeedsSky) |
            ToFeatureMask(MaterialFeature::SurfaceStandard) |
            ToFeatureMask(MaterialFeature::LightingPBR);
        
        uint64_t hash1 = key1.Hash();
        uint64_t hash2 = key2.Hash();
        
        fprintf(stdout, "[MaterialVariantKeyCacheKeyTest] Test Case 1 (Old vs New Path):\n");
        fprintf(stdout, "  Old path hash (no effective_feature_mask): 0x%016llx\n", (unsigned long long)hash1);
        fprintf(stdout, "  New path hash (with effective_feature_mask): 0x%016llx\n", (unsigned long long)hash2);
        
        if (hash1 == hash2)
        {
            fprintf(stderr, "[MaterialVariantKeyCacheKeyTest] FAIL: Hashes should differ when effective_feature_mask changes\n");
            return 1;
        }
        
        fprintf(stdout, "  ✓ Hashes differ correctly\n");
    }

    // Test Case 2: Equivalence within same feature path (both with effective_feature_mask)
    {
        MaterialVariantKey key1, key2;
        
        uint64_t feature_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::NeedsSky) |
            ToFeatureMask(MaterialFeature::SurfaceStandard) |
            ToFeatureMask(MaterialFeature::LightingPBR);
        
        key1.surface_type = SurfaceType::Standard;
        key1.lighting_model = LightingModel::PBR;
        key1.effective_feature_mask = feature_mask;
        
        key2.surface_type = SurfaceType::Standard;
        key2.lighting_model = LightingModel::PBR;
        key2.effective_feature_mask = feature_mask;
        
        uint64_t hash1 = key1.Hash();
        uint64_t hash2 = key2.Hash();
        
        fprintf(stdout, "[MaterialVariantKeyCacheKeyTest] Test Case 2 (Same Feature Path):\n");
        fprintf(stdout, "  Key1 hash: 0x%016llx\n", (unsigned long long)hash1);
        fprintf(stdout, "  Key2 hash: 0x%016llx\n", (unsigned long long)hash2);
        
        if (hash1 != hash2)
        {
            fprintf(stderr, "[MaterialVariantKeyCacheKeyTest] FAIL: Hashes should match for identical configs\n");
            return 1;
        }
        
        fprintf(stdout, "  ✓ Hashes match for identical configs\n");
    }

    // Test Case 3: Operator== consistency with Hash()
    {
        MaterialVariantKey key1, key2;
        
        key1.surface_type = SurfaceType::Standard;
        key1.lighting_model = LightingModel::PBR;
        key1.effective_feature_mask = 0;
        
        key2.surface_type = SurfaceType::Standard;
        key2.lighting_model = LightingModel::PBR;
        key2.effective_feature_mask = 0;
        
        uint64_t hash1 = key1.Hash();
        uint64_t hash2 = key2.Hash();
        bool equal = (key1 == key2);
        
        fprintf(stdout, "[MaterialVariantKeyCacheKeyTest] Test Case 3 (Operator== Consistency):\n");
        fprintf(stdout, "  Keys equal: %s\n", equal ? "true" : "false");
        fprintf(stdout, "  Hashes match: %s\n", (hash1 == hash2) ? "true" : "false");
        
        if (equal != (hash1 == hash2))
        {
            fprintf(stderr, "[MaterialVariantKeyCacheKeyTest] FAIL: operator== and Hash() inconsistent\n");
            return 1;
        }
        
        fprintf(stdout, "  ✓ operator== and Hash() consistent\n");
    }

    // Test Case 4: Hash-based unordered_map cache simulation
    {
        std::unordered_map<uint64_t, MaterialVariantKey> cache;
        
        MaterialVariantKey key_old, key_new;
        key_old.surface_type = SurfaceType::Standard;
        key_old.lighting_model = LightingModel::PBR;
        key_old.effective_feature_mask = 0;
        
        key_new.surface_type = SurfaceType::Standard;
        key_new.lighting_model = LightingModel::PBR;
        key_new.effective_feature_mask = 
            ToFeatureMask(MaterialFeature::NeedsCamera) |
            ToFeatureMask(MaterialFeature::NeedsSky) |
            ToFeatureMask(MaterialFeature::SurfaceStandard) |
            ToFeatureMask(MaterialFeature::LightingPBR);
        
        uint64_t hash_old = key_old.Hash();
        uint64_t hash_new = key_new.Hash();
        
        cache[hash_old] = key_old;
        
        fprintf(stdout, "[MaterialVariantKeyCacheKeyTest] Test Case 4 (Cache Simulation):\n");
        fprintf(stdout, "  Stored old path: hash=0x%016llx\n", (unsigned long long)hash_old);
        fprintf(stdout, "  Query new path: hash=0x%016llx\n", (unsigned long long)hash_new);
        
        bool old_found = (cache.find(hash_old) != cache.end());
        bool new_found = (cache.find(hash_new) != cache.end());
        
        fprintf(stdout, "  Old path found in cache: %s\n", old_found ? "yes" : "no");
        fprintf(stdout, "  New path found in cache: %s\n", new_found ? "yes" : "no");
        
        if (old_found && new_found)
        {
            fprintf(stderr, "[MaterialVariantKeyCacheKeyTest] FAIL: Cache should distinguish old vs new path\n");
            return 1;
        }
        
        fprintf(stdout, "  ✓ Cache correctly separates old and new paths\n");
    }

    fprintf(stdout, "[MaterialVariantKeyCacheKeyTest] PASS\n");
    return 0;
}
