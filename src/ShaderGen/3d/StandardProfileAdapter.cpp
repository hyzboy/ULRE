#include "StandardProfileAdapter.h"
#include <hgl/mtl/MaterialRuntimeContext.h>


namespace hgl::graph::mtl
{

MaterialProfileAsset BuildBuiltinStandardDefaultProfile()
{
    MaterialProfileAsset profile;
    profile.profile_id = "Standard_Default";
    profile.semantic_preset = MaterialPreset::Standard;
    profile.schema_version = 1;

    MaterialSlotPolicy base_color;
    base_color.slot = SamplerSlot::BaseColor;
    base_color.required = true;
    base_color.allowed_modes = { TextureSourceMode::Simple, TextureSourceMode::Array };
    base_color.default_mode = TextureSourceMode::Simple;
    base_color.fallback_chain = { TextureSourceMode::Simple };

    MaterialSlotPolicy normal;
    normal.slot = SamplerSlot::Normal;
    normal.required = false;
    normal.allowed_modes = { TextureSourceMode::Simple, TextureSourceMode::Array };
    normal.default_mode = TextureSourceMode::Simple;
    normal.fallback_chain = { TextureSourceMode::Simple };

    profile.slots.push_back(base_color);
    profile.slots.push_back(normal);

    // Stage D.2: Bootstrap single candidate implementation (Standard)
    MaterialImplementationCandidate std_impl;
    std_impl.impl_id = "Standard";
    std_impl.surface = SurfaceType::Standard;
    // No slot mode requirements: Standard works with both Simple and Array modes

    profile.implementations.push_back(std_impl);
    return profile;
}

bool BuildStandardPolicyFromProfile(const MaterialProfileAsset &profile,
                                    const MaterialVariantKey &input_key,
                                    StandardVariantPolicyResult &out_policy,
                                    std::vector<std::string> &diagnostics)
{
    diagnostics.clear();

    if (profile.semantic_preset != MaterialPreset::Standard)
    {
        diagnostics.push_back("[StandardProfileAdapter] profile semantic_preset is not Standard");
        return false;
    }

    if (!ValidateMaterialProfileAsset(profile, diagnostics))
        return false;

    // Stage-B behavior-equivalent adapter: preserve current Standard policy behavior.
    out_policy = BuildStandardVariantPolicy(input_key);
    return true;
}

} // namespace hgl::graph::mtl
