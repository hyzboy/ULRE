#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>

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

    /// 语义 X 列表（单一真源——枚举与 GetDescriptorSemanticName 同源，新增语义只改此处）
    /// 分组：per-draw 参数表 / 场景 UBO / 变换 / 材质私有数据 / 材质纹理 / 顶点数据 SSBO / 调色板
#define HGL_DESCRIPTOR_SEMANTIC_LIST \
    HGL_SEMANTIC(Unknown) \
    /* mesh per-draw 参数表（IndirectMeshDraw：所有 mesh 材质必备）*/ \
    HGL_SEMANTIC(MeshDrawParams) \
    HGL_SEMANTIC(ViewportInfo) \
    HGL_SEMANTIC(CameraInfo) \
    HGL_SEMANTIC(SkyInfo) \
    HGL_SEMANTIC(LocalToWorld) \
    HGL_SEMANTIC(LocalToWorldIndex) \
    /* per-instance SSBO 私有数据槽（单槽，slot 0）*/ \
    HGL_SEMANTIC(MaterialPrivateData) \
    HGL_SEMANTIC(MaterialPrivateDataIndex) \
    HGL_SEMANTIC(MaterialTexture) \
    HGL_SEMANTIC(MaterialSampler) \
    HGL_SEMANTIC(MaterialTextureLayerTable) \
    /* 顶点数据 SSBO（mesh 是唯一顶点路径，顶点输入统一为 SSBO）*/ \
    HGL_SEMANTIC(VertexPosition) \
    HGL_SEMANTIC(VertexUV) \
    HGL_SEMANTIC(VertexNTB) \
    HGL_SEMANTIC(VertexColor) \
    HGL_SEMANTIC(VertexLuminance) \
    HGL_SEMANTIC(VertexTransformID) \
    HGL_SEMANTIC(VertexSize) \
    HGL_SEMANTIC(VertexIndex) \
    HGL_SEMANTIC(MaterialColorPalette)

    enum class DescriptorSemantic : uint8
    {
#define HGL_SEMANTIC(name) name,
        HGL_DESCRIPTOR_SEMANTIC_LIST
#undef HGL_SEMANTIC

        ENUM_CLASS_RANGE(Unknown,MaterialColorPalette)
    };

    /// 语义名（诊断/校验消息用）。与枚举同源：新增语义无需改本函数。
    /// ⚠️ 2026-08-31 修正：原实现（ShaderResourceSchema.h 手写 switch）漏了全部 8 个
    /// Vertex* 语义 → 顶点资源的诊断消息一律显示 "Unknown"；且 MaterialPrivateData
    /// 错返 GLSL buffer 名 "mtl_private_data" 而非枚举名。X 列表化后不可能再漏。
    inline const char *GetDescriptorSemanticName(const DescriptorSemantic semantic) noexcept
    {
#define HGL_SEMANTIC(name) case DescriptorSemantic::name: return #name;
        switch (semantic)
        {
            HGL_DESCRIPTOR_SEMANTIC_LIST
        }
#undef HGL_SEMANTIC

        return "Unknown";
    }

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

            // 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）
            case DescriptorSemantic::VertexPosition:
            case DescriptorSemantic::VertexUV:
            case DescriptorSemantic::VertexNTB:
            case DescriptorSemantic::VertexColor:
            case DescriptorSemantic::VertexLuminance:
            case DescriptorSemantic::VertexTransformID:
            case DescriptorSemantic::VertexSize:
            case DescriptorSemantic::VertexIndex:

                return DescriptorSemanticLayer::SSBO;

            case DescriptorSemantic::MaterialColorPalette:
                return DescriptorSemanticLayer::UBO;
        }

        return DescriptorSemanticLayer::Unknown;
    }

}
