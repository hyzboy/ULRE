#include <hgl/mtl/MaterialVariantRegistry.h>
#include <hgl/mtl/MaterialVariantKey.h>

#include <cstdio>
#include <utility>
#include <vector>

using namespace hgl::graph::mtl;

namespace
{
MaterialVariantDesc MakeDesc(const char *name)
{
    MaterialVariantDesc d;
    d.variant_name = name ? name : "<unnamed>";
    d.vs_template_path = "compositor/main_forward_lit.vert.glsl";
    d.fs_template_path = "compositor/main_forward_lit.frag.glsl";
    d.surface_function_path = "surface/standard_surface.glsl";
    return d;
}

MaterialVariantKey MakeStandardKey(const LightingModel lighting)
{
    MaterialVariantKey k;
    k.surface_type = hgl::graph::SurfaceType::Standard;
    k.geometry_mode = GeometryMode::Mesh3D;
    k.lighting_model = lighting;
    k.sky_ambient_model = SkyLightAmbientModel::Simple;
    return k;
}
} // anonymous namespace

int main()
{
    std::fprintf(stdout, "[VariantRegistryDescAgnosticRoutingTest] Start\n");

    VariantRegistry registry;
    const MaterialVariantKey lambert = MakeStandardKey(LightingModel::Lambert);
    const MaterialVariantKey pbr = MakeStandardKey(LightingModel::PBR);

    // Store desc objects in a container first to exercise copy/move paths
    // before registration, ensuring lookup correctness is key-only.
    std::vector<std::pair<MaterialVariantKey, MaterialVariantDesc>> entries;
    entries.emplace_back(lambert, MakeDesc("StandardLambert"));
    entries.emplace_back(pbr, MakeDesc("StandardPBR"));

    for (const auto &entry : entries)
        registry.RegisterVariant(entry.first, entry.second);

    MaterialVariantKey resolved{};
    const MaterialVariantDesc *desc = registry.QueryVariantWithCanonicalFallback(pbr, &resolved);
    if (!desc)
    {
        std::fprintf(stderr, "[VariantRegistryDescAgnosticRoutingTest] FAIL: PBR query returned null\n");
        return 1;
    }

    if (desc->variant_name != "StandardPBR")
    {
        std::fprintf(stderr,
                     "[VariantRegistryDescAgnosticRoutingTest] FAIL: expected StandardPBR, got %s\n",
                     desc->variant_name.c_str());
        return 1;
    }

    if (resolved.lighting_model != LightingModel::PBR)
    {
        std::fprintf(stderr,
                     "[VariantRegistryDescAgnosticRoutingTest] FAIL: resolved lighting model should be PBR\n");
        return 1;
    }

    std::fprintf(stdout, "[VariantRegistryDescAgnosticRoutingTest] PASS\n");
    return 0;
}
