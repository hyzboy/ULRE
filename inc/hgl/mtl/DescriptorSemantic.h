#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/DescriptorKind.h>

namespace hgl::graph::mtl
{
    enum class DescriptorSemanticLayer : uint8
    {
        Unknown = 0,
        UBO,
        SSBO,
        Texture,
        Sampler
    };

    enum class DescriptorSemantic : uint8
    {
        Unknown = 0,

        ViewportInfo,
        CameraInfo,
        SkyInfo,
        SkyCubemapSampler,

        LocalToWorld,
        LocalToWorldIndexTable,
        MaterialInstance,
        MaterialColorPalette,

        MaterialTexture,
        MaterialSampler,
        MaterialTextureLayerTable,
        MaterialSSBOIndexTable,

        Custom,
    };

    enum class UBODescriptorSemantic : uint8
    {
        ViewportInfo = 0,
        CameraInfo,
        SkyInfo,
        LocalToWorld,
        MaterialInstance,
        MaterialColorPalette
    };

    enum class SSBODescriptorSemantic : uint8
    {
        LocalToWorld = 0,
        LocalToWorldIndexTable,
        MaterialInstance,
        MaterialTextureLayerTable,
        MaterialSSBOIndexTable
    };

    inline const char *GetDescriptorSemanticLayerName(const DescriptorSemanticLayer layer)
    {
        switch (layer)
        {
        case DescriptorSemanticLayer::Unknown: return "Unknown";
        case DescriptorSemanticLayer::UBO: return "UBO";
        case DescriptorSemanticLayer::SSBO: return "SSBO";
        case DescriptorSemanticLayer::Texture: return "Texture";
        case DescriptorSemanticLayer::Sampler: return "Sampler";
        }

        return "Unknown";
    }

    inline DescriptorSemanticLayer GetDescriptorSemanticLayer(const DescriptorSemantic semantic)
    {
        switch (semantic)
        {
        case DescriptorSemantic::ViewportInfo:
        case DescriptorSemantic::CameraInfo:
        case DescriptorSemantic::SkyInfo:
        case DescriptorSemantic::MaterialColorPalette:
            return DescriptorSemanticLayer::UBO;

        case DescriptorSemantic::MaterialTexture:
            return DescriptorSemanticLayer::Texture;

        case DescriptorSemantic::SkyCubemapSampler:
        case DescriptorSemantic::MaterialSampler:
            return DescriptorSemanticLayer::Sampler;

        case DescriptorSemantic::LocalToWorldIndexTable:
        case DescriptorSemantic::MaterialTextureLayerTable:
        case DescriptorSemantic::MaterialSSBOIndexTable:
            return DescriptorSemanticLayer::SSBO;

        case DescriptorSemantic::LocalToWorld:
        case DescriptorSemantic::MaterialInstance:
        case DescriptorSemantic::Unknown:
        case DescriptorSemantic::Custom:
            return DescriptorSemanticLayer::Unknown;
        }

        return DescriptorSemanticLayer::Unknown;
    }


    constexpr DescriptorSemanticLayer GetDescriptorSemanticLayerByKind(const DescriptorKind kind)
    {
        switch (kind)
        {
        case DescriptorKind::UBO: return DescriptorSemanticLayer::UBO;
        case DescriptorKind::SSBO: return DescriptorSemanticLayer::SSBO;
        case DescriptorKind::Texture: return DescriptorSemanticLayer::Texture;
        case DescriptorKind::TextureSampler: return DescriptorSemanticLayer::Sampler;
        default: return DescriptorSemanticLayer::Unknown;
        }
    }
    inline bool TryGetUBODescriptorSemantic(const DescriptorSemantic semantic, UBODescriptorSemantic &out)
    {
        switch (semantic)
        {
        case DescriptorSemantic::ViewportInfo: out = UBODescriptorSemantic::ViewportInfo; return true;
        case DescriptorSemantic::CameraInfo: out = UBODescriptorSemantic::CameraInfo; return true;
        case DescriptorSemantic::SkyInfo: out = UBODescriptorSemantic::SkyInfo; return true;
        case DescriptorSemantic::LocalToWorld: out = UBODescriptorSemantic::LocalToWorld; return true;
        case DescriptorSemantic::MaterialInstance: out = UBODescriptorSemantic::MaterialInstance; return true;
        case DescriptorSemantic::MaterialColorPalette: out = UBODescriptorSemantic::MaterialColorPalette; return true;
        default: break;
        }

        return false;
    }

    inline bool TryGetSSBODescriptorSemantic(const DescriptorSemantic semantic, SSBODescriptorSemantic &out)
    {
        switch (semantic)
        {
        case DescriptorSemantic::LocalToWorld: out = SSBODescriptorSemantic::LocalToWorld; return true;
        case DescriptorSemantic::LocalToWorldIndexTable: out = SSBODescriptorSemantic::LocalToWorldIndexTable; return true;
        case DescriptorSemantic::MaterialInstance: out = SSBODescriptorSemantic::MaterialInstance; return true;
        case DescriptorSemantic::MaterialTextureLayerTable: out = SSBODescriptorSemantic::MaterialTextureLayerTable; return true;
        case DescriptorSemantic::MaterialSSBOIndexTable: out = SSBODescriptorSemantic::MaterialSSBOIndexTable; return true;
        default: break;
        }

        return false;
    }
}


