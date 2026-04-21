#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/MaterialPreset.h>

#include <cstdio>
#include <cstdint>

using namespace hgl::graph::mtl;

namespace
{
    bool CheckCase(const char *name,
                   const MaterialFeatureMask mask,
                   const FeatureAwareFallbackClass expected_class,
                   const MaterialPreset expected_primary,
                   const MaterialPreset expected_secondary)
    {
        const FeatureAwareFallbackClass got_class = ClassifyFallbackFeatureMask(mask);
        const MaterialPreset got_primary = GetPrimaryFallbackPreset(got_class);
        const MaterialPreset got_secondary = GetSecondaryFallbackPreset(got_class);

        const bool pass = (got_class == expected_class)
                       && (got_primary == expected_primary)
                       && (got_secondary == expected_secondary);

        std::fprintf(stdout,
                     "[FeatureAwareFallbackRoutingTest] %s mask=0x%016llx class=%s primary=%u secondary=%u => %s\n",
                     name,
                     static_cast<unsigned long long>(mask),
                     GetFeatureAwareFallbackClassName(got_class),
                     static_cast<unsigned>(got_primary),
                     static_cast<unsigned>(got_secondary),
                     pass ? "PASS" : "FAIL");

        return pass;
    }
}

int main()
{
    std::fprintf(stdout, "[FeatureAwareFallbackRoutingTest] Starting Phase 7 fallback routing validation...\n");

    bool ok = true;

    ok = ok && CheckCase("GenericZero",
                         0,
                         FeatureAwareFallbackClass::Generic,
                         MaterialPreset::Checkerboard3D,
                         MaterialPreset::Standard);

    ok = ok && CheckCase("SkyByNeed",
                         ToFeatureMask(MaterialFeature::NeedsSky),
                         FeatureAwareFallbackClass::Sky,
                         MaterialPreset::SkyMinimal,
                         MaterialPreset::Standard);

    ok = ok && CheckCase("SkyBySurface",
                         ToFeatureMask(MaterialFeature::SurfaceSky),
                         FeatureAwareFallbackClass::Sky,
                         MaterialPreset::SkyMinimal,
                         MaterialPreset::Standard);

    ok = ok && CheckCase("LitByEnable",
                         ToFeatureMask(MaterialFeature::EnableLighting),
                         FeatureAwareFallbackClass::Lit,
                         MaterialPreset::Standard,
                         MaterialPreset::Checkerboard3D);

    ok = ok && CheckCase("LitByImpl",
                         ToFeatureMask(MaterialFeature::LightingPBR),
                         FeatureAwareFallbackClass::Lit,
                         MaterialPreset::Standard,
                         MaterialPreset::Checkerboard3D);

    ok = ok && CheckCase("LitByTerrain",
                         ToFeatureMask(MaterialFeature::SurfaceTerrain),
                         FeatureAwareFallbackClass::Lit,
                         MaterialPreset::Standard,
                         MaterialPreset::Checkerboard3D);

    ok = ok && CheckCase("Unlit",
                         ToFeatureMask(MaterialFeature::SurfaceUnlit),
                         FeatureAwareFallbackClass::Unlit,
                         MaterialPreset::PureColor3D,
                         MaterialPreset::Checkerboard3D);

    if (!ok)
    {
        std::fprintf(stdout, "[FeatureAwareFallbackRoutingTest] FAIL\n");
        return 1;
    }

    std::fprintf(stdout, "[FeatureAwareFallbackRoutingTest] PASS\n");
    return 0;
}
