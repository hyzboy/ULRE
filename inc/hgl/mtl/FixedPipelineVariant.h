#pragma once

#include <hgl/mtl/RenderTemplate.h>

namespace hgl::graph::mtl
{
    enum class FixedPipelineFamily : uint8
    {
        Unknown = 0,
        ForwardLit,
        ForwardUnlit,
        ShadowCaster,
        Sky,
        Decal,
        PostProcess
    };

    enum class FixedShaderProfile : uint8
    {
        Unknown = 0,
        ForwardLitPBRIBLRGBA16F2,
        ForwardLitPBRSHRG8,
        ForwardLitFakePBRSH,
        ForwardLitBlinnPhongEnvMap,
        ForwardUnlitPureColor,
        ForwardUnlitVertexColor,
        ForwardUnlitTexture,
        ShadowCasterOpaque,
        ShadowCasterMasked,
        SkyConstant,
        SkyEnvMap,
        SkyAtmosphere,
        DecalProjected,
        PostProcessSSAO,
        PostProcessDOF
    };

    enum class FixedShaderQualityTier : uint8
    {
        Default = 0,
        Low,
        Medium,
        High
    };

    using FixedShaderProfileMask = uint64;

    constexpr FixedShaderProfileMask GetFixedShaderProfileMask(
        const FixedShaderProfile profile) noexcept
    {
        return profile == FixedShaderProfile::Unknown
            ? 0
            : FixedShaderProfileMask(1) << (uint8(profile) - 1);
    }

    struct FixedShaderVariantKey
    {
        FixedPipelineFamily family = FixedPipelineFamily::Unknown;
        FixedShaderProfile profile = FixedShaderProfile::Unknown;
        FixedShaderQualityTier quality_tier =
            FixedShaderQualityTier::Default;
    };

    struct FixedPipelineVariant
    {
        FixedShaderVariantKey key;
        RenderTemplateID fragment_template = RenderTemplateID::Unknown;
        uint32 template_version = 0;
    };

    const char *GetFixedPipelineFamilyName(
        FixedPipelineFamily family) noexcept;
    const char *GetFixedShaderProfileName(
        FixedShaderProfile profile) noexcept;

    const FixedPipelineVariant *ResolveFixedPipelineVariant(
        const FixedShaderVariantKey &key) noexcept;

    const FixedPipelineVariant *ResolveFixedPipelineVariantForQuality(
        FixedPipelineFamily family,
        FixedShaderProfileMask allowed_profiles,
        FixedShaderProfile default_profile,
        FixedShaderQualityTier quality_tier) noexcept;

    bool IsFixedShaderProfileAllowed(
        FixedShaderProfileMask allowed_profiles,
        FixedShaderProfile profile) noexcept;
}
