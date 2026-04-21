#include <hgl/mtl/MaterialVariantRegistry.h>
#include <hgl/mtl/MaterialVariantKey.h>

#include <cstdio>

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
    std::fprintf(stdout, "[VariantRegistryRoutingBehaviorTest] Start\n");

    VariantRegistry registry;

    const MaterialVariantKey lambert = MakeStandardKey(LightingModel::Lambert);
    const MaterialVariantKey pbr = MakeStandardKey(LightingModel::PBR);

    registry.RegisterVariant(lambert, MakeDesc("StandardLambert"));
    registry.RegisterVariant(pbr, MakeDesc("StandardPBR"));

    // Case 1: lighting model exact match should resolve to PBR variant.
    {
        MaterialVariantKey resolved{};
        const MaterialVariantDesc *desc = registry.QueryVariantWithCanonicalFallback(pbr, &resolved);
        if (!desc)
        {
            std::fprintf(stderr, "[VariantRegistryRoutingBehaviorTest] FAIL: PBR query returned null\n");
            return 1;
        }
        if (desc->variant_name != "StandardPBR")
        {
            std::fprintf(stderr,
                "[VariantRegistryRoutingBehaviorTest] FAIL: expected StandardPBR, got %s\n",
                desc->variant_name.c_str());
            return 1;
        }
        if (resolved.lighting_model != LightingModel::PBR)
        {
            std::fprintf(stderr,
                "[VariantRegistryRoutingBehaviorTest] FAIL: resolved key lighting model not PBR\n");
            return 1;
        }
        std::fprintf(stdout, "  Case1 OK: exact lighting route hit PBR\n");
    }

    // Case 2: effective_feature_mask is ignored by default in registry routing.
    {
        MaterialVariantKey request = pbr;
        request.effective_feature_mask = 0x1234ull;

        MaterialVariantKey resolved{};
        const MaterialVariantDesc *desc = registry.QueryVariantWithCanonicalFallback(request, &resolved);
        if (!desc)
        {
            std::fprintf(stderr, "[VariantRegistryRoutingBehaviorTest] FAIL: masked request returned null\n");
            return 1;
        }
        if (desc->variant_name != "StandardPBR")
        {
            std::fprintf(stderr,
                "[VariantRegistryRoutingBehaviorTest] FAIL: expected StandardPBR with mask ignored, got %s\n",
                desc->variant_name.c_str());
            return 1;
        }
        if (resolved.effective_feature_mask != 0)
        {
            std::fprintf(stderr,
                "[VariantRegistryRoutingBehaviorTest] FAIL: resolved key effective mask should be canonicalized to 0\n");
            return 1;
        }
        std::fprintf(stdout, "  Case2 OK: effective_feature_mask ignored by default routing\n");
    }

    // Case 3: fallback step0 still works for legacy requests where PBR variant is absent.
    {
        VariantRegistry legacy_registry;
        legacy_registry.RegisterVariant(lambert, MakeDesc("StandardLambertOnly"));

        MaterialVariantKey request = pbr;
        MaterialVariantKey resolved{};
        const MaterialVariantDesc *desc = legacy_registry.QueryVariantWithCanonicalFallback(request, &resolved);
        if (!desc)
        {
            std::fprintf(stderr, "[VariantRegistryRoutingBehaviorTest] FAIL: fallback query returned null\n");
            return 1;
        }
        if (desc->variant_name != "StandardLambertOnly")
        {
            std::fprintf(stderr,
                "[VariantRegistryRoutingBehaviorTest] FAIL: expected fallback to StandardLambertOnly, got %s\n",
                desc->variant_name.c_str());
            return 1;
        }
        if (resolved.lighting_model != LightingModel::Lambert)
        {
            std::fprintf(stderr,
                "[VariantRegistryRoutingBehaviorTest] FAIL: fallback resolved key should canonicalize to Lambert\n");
            return 1;
        }
        std::fprintf(stdout, "  Case3 OK: step0 fallback remains functional\n");
    }

    // Case 4: removed step2 means sampler-bit-only mismatches should now miss.
    {
        VariantRegistry no_step2_registry;

        MaterialVariantKey coarse = lambert;
        coarse.texture_source_bits = 0;
        coarse.sampler_feature_bits = 0;
        no_step2_registry.RegisterVariant(coarse, MakeDesc("CoarseLambert"));

        MaterialVariantKey request = lambert;
        request.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);

        MaterialVariantKey resolved{};
        const MaterialVariantDesc *desc = no_step2_registry.QueryVariantWithCanonicalFallback(request, &resolved);
        if (desc)
        {
            std::fprintf(stderr,
                "[VariantRegistryRoutingBehaviorTest] FAIL: expected miss without step2, got %s\n",
                desc->variant_name.c_str());
            return 1;
        }
        std::fprintf(stdout, "  Case4 OK: sampler-bit mismatch no longer falls through step2\n");
    }

    // Case 5: removed step1 means texture-source mismatch should now miss.
    {
        VariantRegistry no_step1_registry;

        MaterialVariantKey simple = lambert;
        simple.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
        no_step1_registry.RegisterVariant(simple, MakeDesc("SimpleLambert"));

        MaterialVariantKey request = lambert;
        request.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Array);

        MaterialVariantKey resolved{};
        const MaterialVariantDesc *desc = no_step1_registry.QueryVariantWithCanonicalFallback(request, &resolved);
        if (desc)
        {
            std::fprintf(stderr,
                "[VariantRegistryRoutingBehaviorTest] FAIL: expected miss without step1, got %s\n",
                desc->variant_name.c_str());
            return 1;
        }
        std::fprintf(stdout, "  Case5 OK: texture-source mismatch no longer falls through step1\n");
    }

    std::fprintf(stdout, "[VariantRegistryRoutingBehaviorTest] PASS\n");
    return 0;
}
