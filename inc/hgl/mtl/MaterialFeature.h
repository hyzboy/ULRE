#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/LightingModel.h>
#include <string>
#include <cstdio>

namespace hgl::graph::mtl
{
    enum class MaterialFeature : uint64
    {
        None                = 0,

        // Resource requirements
        NeedsCamera         = 1ull << 0,
        NeedsSky            = 1ull << 1,
        NeedsLocalToWorld   = 1ull << 2,
        NeedsMaterialInst   = 1ull << 3,

        // Functional toggles
        EnableLighting      = 1ull << 4,
        AlphaMasked         = 1ull << 5,
        AlphaDither         = 1ull << 6,
        TextureArrayMode    = 1ull << 7,

        // Surface family (intended as one-of group)
        SurfaceUnlit        = 1ull << 12,
        SurfaceStandard     = 1ull << 13,
        SurfaceTerrain      = 1ull << 14,
        SurfaceSky          = 1ull << 15,

        // Lighting implementation (zero or one is recommended)
        LightingLambert     = 1ull << 20,
        LightingBlinnPhong  = 1ull << 21,
        LightingPBR         = 1ull << 22,
    };

    using MaterialFeatureMask = uint64;

    struct MaterialFeatureValidationResult
    {
        bool well_formed = true;
        bool has_surface_conflict = false;
        bool has_lighting_impl_conflict = false;
    };

    constexpr MaterialFeatureMask ToFeatureMask(const MaterialFeature f)
    {
        return static_cast<MaterialFeatureMask>(f);
    }

    constexpr bool HasFeature(const MaterialFeatureMask mask, const MaterialFeature f)
    {
        return (mask & ToFeatureMask(f)) != 0;
    }

    constexpr bool HasAnyFeature(const MaterialFeatureMask mask, const MaterialFeatureMask test_bits)
    {
        return (mask & test_bits) != 0;
    }

    constexpr MaterialFeatureMask AddFeature(const MaterialFeatureMask mask, const MaterialFeature f)
    {
        return mask | ToFeatureMask(f);
    }

    constexpr MaterialFeatureMask RemoveFeature(const MaterialFeatureMask mask, const MaterialFeature f)
    {
        return mask & (~ToFeatureMask(f));
    }

        constexpr MaterialFeatureMask SurfaceFeatureMask =
                static_cast<MaterialFeatureMask>(MaterialFeature::SurfaceUnlit)
            + static_cast<MaterialFeatureMask>(MaterialFeature::SurfaceStandard)
            + static_cast<MaterialFeatureMask>(MaterialFeature::SurfaceTerrain)
            + static_cast<MaterialFeatureMask>(MaterialFeature::SurfaceSky);

        constexpr MaterialFeatureMask LightingImplFeatureMask =
                static_cast<MaterialFeatureMask>(MaterialFeature::LightingLambert)
            + static_cast<MaterialFeatureMask>(MaterialFeature::LightingBlinnPhong)
            + static_cast<MaterialFeatureMask>(MaterialFeature::LightingPBR);

    constexpr uint32 CountFeatureBits(const MaterialFeatureMask mask)
    {
        uint32 count = 0;
        MaterialFeatureMask x = mask;

        while (x != 0)
        {
            x &= (x - 1);
            ++count;
        }

        return count;
    }

    constexpr MaterialFeatureValidationResult ValidateFeatureMask(const MaterialFeatureMask mask)
    {
        MaterialFeatureValidationResult result{};

        const uint32 surface_count = CountFeatureBits(mask & SurfaceFeatureMask);
        if (surface_count > 1)
        {
            result.well_formed = false;
            result.has_surface_conflict = true;
        }

        const uint32 lighting_impl_count = CountFeatureBits(mask & LightingImplFeatureMask);
        if (lighting_impl_count > 1)
        {
            result.well_formed = false;
            result.has_lighting_impl_conflict = true;
        }

        return result;
    }

    inline std::string BuildMalformedIntentFeatureWarningMessage(const MaterialFeatureMask feature_mask,
                                                                 const MaterialPreset preset,
                                                                 const MaterialFeatureValidationResult &validation)
    {
        char buf[256] = {};

        std::snprintf(buf,
                      sizeof(buf),
                      "[CreateMaterialFromRecord] warning: malformed intent_features=0x%llX preset=%u surface_conflict=%d lighting_impl_conflict=%d",
                      static_cast<unsigned long long>(feature_mask),
                      static_cast<unsigned>(preset),
                      validation.has_surface_conflict ? 1 : 0,
                      validation.has_lighting_impl_conflict ? 1 : 0);

        return std::string(buf);
    }

    inline constexpr MaterialFeatureMask GetDefaultIntentFeatureMask(const MaterialPreset preset)
    {
        using MF = MaterialFeature;

        switch (preset)
        {
            case MaterialPreset::SkyMinimal:
            {
                MaterialFeatureMask mask = ToFeatureMask(MF::NeedsCamera);
                mask = AddFeature(mask, MF::NeedsSky);
                mask = AddFeature(mask, MF::NeedsLocalToWorld);
                mask = AddFeature(mask, MF::SurfaceSky);
                return mask;
            }

            case MaterialPreset::TerrainGrid:
            {
                MaterialFeatureMask mask = ToFeatureMask(MF::NeedsCamera);
                mask = AddFeature(mask, MF::NeedsSky);
                mask = AddFeature(mask, MF::NeedsLocalToWorld);
                mask = AddFeature(mask, MF::EnableLighting);
                mask = AddFeature(mask, MF::SurfaceTerrain);
                mask = AddFeature(mask, MF::LightingLambert);
                return mask;
            }

            case MaterialPreset::Standard:
            case MaterialPreset::PBRColor3D:
            case MaterialPreset::HumanSkin:
            case MaterialPreset::AmphibiansSkin:
            case MaterialPreset::Wood:
            case MaterialPreset::TreeBark:
            case MaterialPreset::Stone:
            case MaterialPreset::Leaf:
            case MaterialPreset::Metal:
            case MaterialPreset::BirdFeathers:
            case MaterialPreset::Scales:
            {
                MaterialFeatureMask mask = ToFeatureMask(MF::NeedsCamera);
                mask = AddFeature(mask, MF::NeedsSky);
                mask = AddFeature(mask, MF::NeedsLocalToWorld);
                mask = AddFeature(mask, MF::EnableLighting);
                mask = AddFeature(mask, MF::SurfaceStandard);
                mask = AddFeature(mask, MF::LightingPBR);
                return mask;
            }

            case MaterialPreset::PureColor2D:
            case MaterialPreset::PureTexture2D:
            case MaterialPreset::VertexColor2D:
            case MaterialPreset::Text2D:
            case MaterialPreset::PureColor3D:
            case MaterialPreset::VertexColor3D:
            case MaterialPreset::VertexLuminance3D:
            case MaterialPreset::VertexLuminance2D:
            case MaterialPreset::VertexPattleColor3D:
            case MaterialPreset::Gizmo3D:
            case MaterialPreset::Checkerboard3D:
            case MaterialPreset::Billboard2DDynamic:
            case MaterialPreset::Billboard2DFixed:
            default:
                return ToFeatureMask(MF::SurfaceUnlit);
        }
    }

    inline constexpr MaterialFeatureMask ResolveIntentFeatureMask(const MaterialPreset preset,
                                                                  const MaterialFeatureMask intent_mask)
    {
        if (intent_mask != 0)
            return intent_mask;

        return GetDefaultIntentFeatureMask(preset);
    }

    inline constexpr LightingModel ResolveLightingModelFromFeatures(const MaterialFeatureMask mask,
                                                                    const LightingModel fallback)
    {
        if (HasFeature(mask, MaterialFeature::LightingPBR))
            return LightingModel::PBR;

        if (HasFeature(mask, MaterialFeature::LightingBlinnPhong))
            return LightingModel::BlinnPhong;

        if (HasFeature(mask, MaterialFeature::LightingLambert))
            return LightingModel::Lambert;

        return fallback;
    }

    enum class FeatureAwareFallbackClass : uint8
    {
        Generic = 0,
        Sky,
        Lit,
        Unlit,
    };

    inline constexpr FeatureAwareFallbackClass ClassifyFallbackFeatureMask(const MaterialFeatureMask mask)
    {
        if (mask == 0)
            return FeatureAwareFallbackClass::Generic;

        if (HasFeature(mask, MaterialFeature::SurfaceSky)
         || HasFeature(mask, MaterialFeature::NeedsSky))
            return FeatureAwareFallbackClass::Sky;

        if (HasFeature(mask, MaterialFeature::EnableLighting)
         || HasAnyFeature(mask, LightingImplFeatureMask)
         || HasFeature(mask, MaterialFeature::SurfaceStandard)
         || HasFeature(mask, MaterialFeature::SurfaceTerrain))
            return FeatureAwareFallbackClass::Lit;

        return FeatureAwareFallbackClass::Unlit;
    }

    inline constexpr MaterialPreset GetPrimaryFallbackPreset(const FeatureAwareFallbackClass fallback_class)
    {
        switch (fallback_class)
        {
            case FeatureAwareFallbackClass::Sky:    return MaterialPreset::SkyMinimal;
            case FeatureAwareFallbackClass::Lit:    return MaterialPreset::Standard;
            case FeatureAwareFallbackClass::Unlit:  return MaterialPreset::PureColor3D;
            case FeatureAwareFallbackClass::Generic:
            default:                                return MaterialPreset::Checkerboard3D;
        }
    }

    inline constexpr MaterialPreset GetSecondaryFallbackPreset(const FeatureAwareFallbackClass fallback_class)
    {
        switch (fallback_class)
        {
            case FeatureAwareFallbackClass::Sky:    return MaterialPreset::Standard;
            case FeatureAwareFallbackClass::Lit:    return MaterialPreset::Checkerboard3D;
            case FeatureAwareFallbackClass::Unlit:  return MaterialPreset::Checkerboard3D;
            case FeatureAwareFallbackClass::Generic:
            default:                                return MaterialPreset::Standard;
        }
    }

    inline constexpr const char *GetFeatureAwareFallbackClassName(const FeatureAwareFallbackClass fallback_class)
    {
        switch (fallback_class)
        {
            case FeatureAwareFallbackClass::Sky:    return "sky";
            case FeatureAwareFallbackClass::Lit:    return "lit";
            case FeatureAwareFallbackClass::Unlit:  return "unlit";
            case FeatureAwareFallbackClass::Generic:
            default:                                return "generic";
        }
    }
}
