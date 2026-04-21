#include <hgl/mtl/MaterialFeature.h>

#include <cstdio>
#include <string>

using namespace hgl::graph::mtl;

int main()
{
    // Intentionally build a malformed mask:
    // - Two surface family bits
    // - Two lighting implementation bits
    MaterialFeatureMask mask = ToFeatureMask(MaterialFeature::SurfaceUnlit);
    mask = AddFeature(mask, MaterialFeature::SurfaceSky);
    mask = AddFeature(mask, MaterialFeature::LightingLambert);
    mask = AddFeature(mask, MaterialFeature::LightingPBR);

    const MaterialFeatureValidationResult validation = ValidateFeatureMask(mask);

    if (validation.well_formed)
    {
        std::fprintf(stderr, "[MaterialFeatureWarningSmoke] expected malformed mask but got well_formed\n");
        return 2;
    }

    const std::string warning = BuildMalformedIntentFeatureWarningMessage(mask,
                                                                          MaterialPreset::Standard,
                                                                          validation);

    std::fprintf(stderr, "%s\n", warning.c_str());

    // Keep this check strict so warning format remains stable for log parsers.
    if (warning.find("[CreateMaterialFromRecord] warning: malformed intent_features=") == std::string::npos)
        return 3;

    if (warning.find("surface_conflict=1") == std::string::npos)
        return 4;

    if (warning.find("lighting_impl_conflict=1") == std::string::npos)
        return 5;

    std::fprintf(stderr, "[MaterialFeatureWarningSmoke] PASS\n");
    return 0;
}
