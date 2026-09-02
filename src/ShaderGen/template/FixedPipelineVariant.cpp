#include <hgl/mtl/FixedPipelineVariant.h>

namespace hgl::graph::mtl
{
    namespace
    {
        constexpr FixedPipelineVariant Variants[] =
        {
            { { FixedPipelineFamily::ForwardLit,
                FixedShaderProfile::ForwardLitPBRIBLRGBA16F2,
                FixedShaderQualityTier::High },
              RenderTemplateID::ForwardLitShadowedAO, 1 },
            { { FixedPipelineFamily::ForwardLit,
                FixedShaderProfile::ForwardLitPBRSHRG8,
                FixedShaderQualityTier::Medium },
              RenderTemplateID::ForwardLitShadowedAO, 1 },
            { { FixedPipelineFamily::ForwardLit,
                FixedShaderProfile::ForwardLitFakePBRSH,
                FixedShaderQualityTier::Low },
              RenderTemplateID::ForwardLitShadowedAO, 1 },
            { { FixedPipelineFamily::ForwardLit,
                FixedShaderProfile::ForwardLitBlinnPhongEnvMap,
                FixedShaderQualityTier::Low },
              RenderTemplateID::ForwardLitShadowedAO, 1 },
            { { FixedPipelineFamily::ForwardUnlit,
                FixedShaderProfile::ForwardUnlitPureColor,
                FixedShaderQualityTier::Default },
              RenderTemplateID::ForwardUnlit, 1 },
            { { FixedPipelineFamily::ForwardUnlit,
                FixedShaderProfile::ForwardUnlitVertexColor,
                FixedShaderQualityTier::Default },
              RenderTemplateID::ForwardUnlit, 1 },
            { { FixedPipelineFamily::ForwardUnlit,
                FixedShaderProfile::ForwardUnlitTexture,
                FixedShaderQualityTier::Default },
              RenderTemplateID::ForwardUnlit, 1 },
            { { FixedPipelineFamily::ShadowCaster,
                FixedShaderProfile::ShadowCasterOpaque,
                FixedShaderQualityTier::Default },
              RenderTemplateID::ShadowCasterOpaque, 1 },
            { { FixedPipelineFamily::ShadowCaster,
                FixedShaderProfile::ShadowCasterMasked,
                FixedShaderQualityTier::Default },
              RenderTemplateID::ShadowCasterMasked, 1 },
            { { FixedPipelineFamily::Sky,
                FixedShaderProfile::SkyConstant,
                FixedShaderQualityTier::Default },
              RenderTemplateID::Sky, 1 },
            { { FixedPipelineFamily::Sky,
                FixedShaderProfile::SkyEnvMap,
                FixedShaderQualityTier::High },
              RenderTemplateID::Sky, 1 },
            { { FixedPipelineFamily::Sky,
                FixedShaderProfile::SkyAtmosphere,
                FixedShaderQualityTier::High },
              RenderTemplateID::Sky, 1 },
            { { FixedPipelineFamily::Decal,
                FixedShaderProfile::DecalProjected,
                FixedShaderQualityTier::Default },
              RenderTemplateID::Decal, 1 },
            { { FixedPipelineFamily::PostProcess,
                FixedShaderProfile::PostProcessSSAO,
                FixedShaderQualityTier::Default },
              RenderTemplateID::PostProcessSSAO, 1 },
            { { FixedPipelineFamily::PostProcess,
                FixedShaderProfile::PostProcessDOF,
                FixedShaderQualityTier::Default },
              RenderTemplateID::PostProcessDOF, 1 }
        };
    }

    const char *GetFixedPipelineFamilyName(
        const FixedPipelineFamily family) noexcept
    {
        switch (family)
        {
        case FixedPipelineFamily::ForwardLit: return "forward_lit";
        case FixedPipelineFamily::ForwardUnlit: return "forward_unlit";
        case FixedPipelineFamily::ShadowCaster: return "shadow_caster";
        case FixedPipelineFamily::Sky: return "sky";
        case FixedPipelineFamily::Decal: return "decal";
        case FixedPipelineFamily::PostProcess: return "postprocess";
        default: return "unknown";
        }
    }

    const char *GetFixedShaderProfileName(
        const FixedShaderProfile profile) noexcept
    {
        switch (profile)
        {
        case FixedShaderProfile::ForwardLitPBRIBLRGBA16F2:
            return "pbr_ibl_rgba16f2";
        case FixedShaderProfile::ForwardLitPBRSHRG8:
            return "pbr_sh_rg8";
        case FixedShaderProfile::ForwardLitFakePBRSH:
            return "fake_pbr_sh";
        case FixedShaderProfile::ForwardLitBlinnPhongEnvMap:
            return "blinn_phong_envmap";
        case FixedShaderProfile::ForwardUnlitPureColor:
            return "pure_color";
        case FixedShaderProfile::ForwardUnlitVertexColor:
            return "vertex_color";
        case FixedShaderProfile::ForwardUnlitTexture:
            return "texture";
        case FixedShaderProfile::ShadowCasterOpaque:
            return "opaque";
        case FixedShaderProfile::ShadowCasterMasked:
            return "masked";
        case FixedShaderProfile::SkyConstant:
            return "constant";
        case FixedShaderProfile::SkyEnvMap:
            return "envmap";
        case FixedShaderProfile::SkyAtmosphere:
            return "atmosphere";
        case FixedShaderProfile::DecalProjected:
            return "projected";
        case FixedShaderProfile::PostProcessSSAO:
            return "ssao";
        case FixedShaderProfile::PostProcessDOF:
            return "dof";
        default: return "unknown";
        }
    }

    const FixedPipelineVariant *ResolveFixedPipelineVariant(
        const FixedShaderVariantKey &key) noexcept
    {
        for (const FixedPipelineVariant &variant : Variants)
        {
            if (variant.key.family == key.family
             && variant.key.profile == key.profile
             && variant.key.quality_tier == key.quality_tier)
                return &variant;
        }
        return nullptr;
    }

    const FixedPipelineVariant *ResolveFixedPipelineVariantForQuality(
        const FixedPipelineFamily family,
        const FixedShaderProfileMask allowed_profiles,
        const FixedShaderProfile default_profile,
        const FixedShaderQualityTier quality_tier) noexcept
    {
        if (quality_tier == FixedShaderQualityTier::Default)
        {
            for (const FixedPipelineVariant &variant : Variants)
            {
                if (variant.key.family == family
                 && variant.key.profile == default_profile
                 && IsFixedShaderProfileAllowed(
                        allowed_profiles, default_profile))
                    return &variant;
            }
            return nullptr;
        }

        for (const FixedPipelineVariant &variant : Variants)
        {
            if (variant.key.family == family
             && variant.key.quality_tier == quality_tier
             && IsFixedShaderProfileAllowed(
                    allowed_profiles, variant.key.profile))
                return &variant;
        }
        return nullptr;
    }

    bool IsFixedShaderProfileAllowed(
        const FixedShaderProfileMask allowed_profiles,
        const FixedShaderProfile profile) noexcept
    {
        return (allowed_profiles & GetFixedShaderProfileMask(profile)) != 0;
    }
}
