#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph::mtl
{
    enum class DescriptorSemanticLayer : uint8
    {
        Unknown = 0,
        UBO,
        SSBO,
        Texture,
        Sampler,

        ENUM_CLASS_RANGE(UBO,Sampler)
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
        MaterialDataIndexTable,

        Custom,
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

        case DescriptorSemantic::LocalToWorldIndexTable:
        case DescriptorSemantic::MaterialTextureLayerTable:
        case DescriptorSemantic::MaterialDataIndexTable:
            return DescriptorSemanticLayer::SSBO;

        case DescriptorSemantic::MaterialTexture:
            return DescriptorSemanticLayer::Texture;

        case DescriptorSemantic::SkyCubemapSampler:
        case DescriptorSemantic::MaterialSampler:
            return DescriptorSemanticLayer::Sampler;

        case DescriptorSemantic::LocalToWorld:
        case DescriptorSemantic::MaterialInstance:
        case DescriptorSemantic::Unknown:
        case DescriptorSemantic::Custom:
            return DescriptorSemanticLayer::Unknown;
        }

        return DescriptorSemanticLayer::Unknown;
    }
}
