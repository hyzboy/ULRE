#pragma once

#include <hgl/type/EnumUtil.h>
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

        // mesh per-draw 参数表（IndirectMeshDraw：所有 mesh 材质必备）
        MeshDrawParams,

        ViewportInfo,
        CameraInfo,
        SkyInfo,

        LocalToWorld,
        LocalToWorldIndex,

        MaterialPrivateData,      // per-instance SSBO slot (one entry per material_private_data_slot_decls[i])
        MaterialPrivateDataIndex,

        MaterialTexture,
        MaterialSampler,
        MaterialTextureLayerTable,

        // 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）
        VertexPosition,
        VertexUV,
        VertexNTB,
        VertexColor,
        VertexLuminance,
        VertexTransformID,
        VertexSize,
        VertexIndex,

        MaterialColorPalette,

        ENUM_CLASS_RANGE(Unknown,MaterialColorPalette)
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
            case DescriptorSemantic::MeshDrawParams:
                return DescriptorSemanticLayer::SSBO;

            case DescriptorSemantic::ViewportInfo:
            case DescriptorSemantic::CameraInfo:
            case DescriptorSemantic::SkyInfo:
                return DescriptorSemanticLayer::UBO;

            case DescriptorSemantic::MaterialTexture:
                return DescriptorSemanticLayer::Texture;

            case DescriptorSemantic::MaterialSampler:
                return DescriptorSemanticLayer::Sampler;

            case DescriptorSemantic::LocalToWorld:
            case DescriptorSemantic::LocalToWorldIndex:

            case DescriptorSemantic::MaterialPrivateData:
            case DescriptorSemantic::MaterialPrivateDataIndex:

            case DescriptorSemantic::MaterialTextureLayerTable:
                return DescriptorSemanticLayer::SSBO;

            case DescriptorSemantic::MaterialColorPalette:
                return DescriptorSemanticLayer::UBO;
        }

        return DescriptorSemanticLayer::Unknown;
    }


    constexpr DescriptorSemanticLayer GetDescriptorSemanticLayerByKind(const DescriptorKind kind)
    {
        switch (kind)
        {
        case DescriptorKind::UBO: return DescriptorSemanticLayer::UBO;
        case DescriptorKind::SSBO: return DescriptorSemanticLayer::SSBO;
        default: return DescriptorSemanticLayer::Unknown;
        }
    }

}

