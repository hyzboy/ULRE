#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/DescriptorSetTypeDef.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/graph/ubo/UBOShaderSources.h>

namespace hgl::graph::mtl
{
    /// 资源类别——与 DescriptorSetType 一一对应：
    /// 全局集按帧绑定（per-material 侧跳过），顶点集冻结几何 ABI，
    /// PerObject 集承载易变行表，Material 集为唯一 per-material 动态路径。
    enum class ResourceCatalogClass : uint8
    {
        SceneGlobal,     ///< Scene 集全局 UBO（P1 已全局化——per-material 注册跳过）
        VertexGeometry,  ///< Vertex 集顶点数据 SSBO（几何 ABI，含 VertexIndex）
        PerDraw,         ///< PerObject 集固定 ABI SSBO（l2w/实例行表/per-draw 参数表）
        MaterialData,    ///< Material 集数据资源（私有数据槽/纹理层表/纹理与采样器声明）
    };

    /// 描述符资源目录——"语义 → 集合/绑定/SBS/VAB 语义"的唯一真源。
    ///
    /// 收敛此前散布的多份平行表：生成侧 kDescriptorRegisterTable、
    /// PushManifestSSBO 的 ssbo_type switch、能力子集校验的无条件允许清单、
    /// ShaderResourceSchema 的 GetExpectedSetType/GetDefaultDescriptorNameBySemantic，
    /// 以及运行时 PipelineMaterialRenderer 的 vab switch/静态表与
    /// RenderDescriptorBindingSystem 的 semantic→VertexSemantic lambda。
    ///
    /// 绑定号一律取自 DescriptorSetTypeDef.h 的绑定枚举（ABI 真源）；
    /// 与 kDescriptorBindingMacros 分工：后者只管 GLSL 宏文本（common 层）。
    struct DescriptorResourceCatalogEntry
    {
        DescriptorSemantic semantic;
        ResourceCatalogClass cls;
        const ShaderBufferSource *sbs;   ///< 固定 SBS 行；nullptr=动态命名（MaterialPrivateData/Texture/Sampler）
        DescriptorSetType set_type;
        int binding;                     ///< 固定绑定号（取自绑定枚举）；-1=per-material 动态
        SSBOType ssbo_type;              ///< UserDefined=无专属类型
        VertexSemantic vab_semantic;     ///< 仅 VertexGeometry 行有效（VAB 语义互查键）；其余 Unknown
        bool engine_builtin;             ///< 能力子集校验：无条件内置允许（false=有条件规则或需声明）
    };

    /// 与各枚举行数对齐的资源目录（行序即发现序，查找线性、语义唯一）。
    constexpr const DescriptorResourceCatalogEntry kDescriptorResourceCatalog[]=
    {
        // ── SceneGlobal：全局 UBO（一帧写/绑一次；binding=SceneBinding 枚举）──
        { DescriptorSemantic::ViewportInfo,         ResourceCatalogClass::SceneGlobal, &SBS_ViewportInfo,  DescriptorSetType::Scene,    int(SceneBinding::Viewport),      SSBOType::UserDefined, VertexSemantic::Unknown, true  },
        { DescriptorSemantic::CameraInfo,           ResourceCatalogClass::SceneGlobal, &SBS_CameraInfo,    DescriptorSetType::Scene,    int(SceneBinding::Camera),        SSBOType::UserDefined, VertexSemantic::Unknown, false },
        { DescriptorSemantic::SkyInfo,              ResourceCatalogClass::SceneGlobal, &SBS_SkyInfo,       DescriptorSetType::Scene,    int(SceneBinding::Sky),           SSBOType::UserDefined, VertexSemantic::Unknown, false },
        { DescriptorSemantic::MaterialColorPalette, ResourceCatalogClass::SceneGlobal, &SBS_ColorPalette,  DescriptorSetType::Scene,    int(SceneBinding::ColorPalette),  SSBOType::UserDefined, VertexSemantic::Unknown, false },

        // ── PerDraw：PerObject 集固定 ABI（易变——行表按批/每 run 更新）──
        { DescriptorSemantic::LocalToWorld,         ResourceCatalogClass::PerDraw, &SBS_LocalToWorld,                   DescriptorSetType::PerObject, int(PerObjectBinding::L2W),              SSBOType::UserDefined,              VertexSemantic::Unknown, false },
        { DescriptorSemantic::LocalToWorldIndex,    ResourceCatalogClass::PerDraw, &SBS_LocalToWorldIndex,              DescriptorSetType::PerObject, int(PerObjectBinding::L2WIndex),         SSBOType::LocalToWorldIndex,        VertexSemantic::Unknown, false },
        { DescriptorSemantic::MeshDrawParams,       ResourceCatalogClass::PerDraw, &SBS_MeshDrawParams,                 DescriptorSetType::PerObject, int(PerObjectBinding::MeshDrawParams),   SSBOType::UserDefined,              VertexSemantic::Unknown, true  },
        { DescriptorSemantic::MaterialPrivateDataIndex, ResourceCatalogClass::PerDraw, &SBS_MaterialPrivateDataIndexRows, DescriptorSetType::PerObject, int(PerObjectBinding::PrivateDataIndex), SSBOType::MaterialPrivateDataIndex, VertexSemantic::Unknown, false },

        // ── VertexGeometry：Vertex 集顶点数据（几何 ABI，长期冻结；vab_semantic 为互查键）──
        { DescriptorSemantic::VertexPosition,    ResourceCatalogClass::VertexGeometry, &SBS_VertexPosition,    DescriptorSetType::Vertex, int(VertexBinding::Position),    SSBOType::VertexPosition,    VertexSemantic::Position,    true },
        { DescriptorSemantic::VertexUV,          ResourceCatalogClass::VertexGeometry, &SBS_VertexUV,          DescriptorSetType::Vertex, int(VertexBinding::UV),          SSBOType::VertexUV,          VertexSemantic::TexCoord,    true },
        { DescriptorSemantic::VertexNTB,         ResourceCatalogClass::VertexGeometry, &SBS_VertexNTB,         DescriptorSetType::Vertex, int(VertexBinding::NTB),         SSBOType::VertexNTB,         VertexSemantic::Normal,      true },
        { DescriptorSemantic::VertexColor,       ResourceCatalogClass::VertexGeometry, &SBS_VertexColor,       DescriptorSetType::Vertex, int(VertexBinding::Color),       SSBOType::VertexColor,       VertexSemantic::Color,       true },
        { DescriptorSemantic::VertexLuminance,   ResourceCatalogClass::VertexGeometry, &SBS_VertexLuminance,   DescriptorSetType::Vertex, int(VertexBinding::Luminance),   SSBOType::VertexLuminance,   VertexSemantic::Luminance,   true },
        { DescriptorSemantic::VertexTransformID, ResourceCatalogClass::VertexGeometry, &SBS_VertexTransformID, DescriptorSetType::Vertex, int(VertexBinding::TransformID), SSBOType::VertexTransformID, VertexSemantic::TransformID, true },
        { DescriptorSemantic::VertexSize,        ResourceCatalogClass::VertexGeometry, &SBS_VertexSize,        DescriptorSetType::Vertex, int(VertexBinding::Size),        SSBOType::VertexSize,        VertexSemantic::Size,        true },
        { DescriptorSemantic::VertexIndex,       ResourceCatalogClass::VertexGeometry, &SBS_VertexIndex,       DescriptorSetType::Vertex, int(VertexBinding::Index),       SSBOType::VertexIndex,       VertexSemantic::Unknown,     true  },

        // ── MaterialData：Material 集（binding=slot/槽数，per-material 动态）──
        { DescriptorSemantic::MaterialPrivateData,       ResourceCatalogClass::MaterialData, nullptr,                     DescriptorSetType::Material, -1, SSBOType::UserDefined,    VertexSemantic::Unknown, false },
        { DescriptorSemantic::MaterialTextureLayerTable, ResourceCatalogClass::MaterialData, &SBS_MaterialTextureLayerRows, DescriptorSetType::Material, -1, SSBOType::TextureLayer, VertexSemantic::Unknown, false },
        { DescriptorSemantic::MaterialTexture,           ResourceCatalogClass::MaterialData, nullptr,                     DescriptorSetType::Material, -1, SSBOType::UserDefined,    VertexSemantic::Unknown, false },
        { DescriptorSemantic::MaterialSampler,           ResourceCatalogClass::MaterialData, nullptr,                     DescriptorSetType::Material, -1, SSBOType::UserDefined,    VertexSemantic::Unknown, false },
    };

    constexpr const size_t DESCRIPTOR_RESOURCE_CATALOG_COUNT=
        sizeof(kDescriptorResourceCatalog)/sizeof(kDescriptorResourceCatalog[0]);

    /// 按语义查找；未收录语义（未知/未来扩展）返回 nullptr
    inline const DescriptorResourceCatalogEntry *FindResourceCatalogEntry(const DescriptorSemantic semantic)
    {
        for (const auto &row : kDescriptorResourceCatalog)
            if (row.semantic == semantic)
                return &row;

        return(nullptr);
    }

    /// 按 SSBO 类型查找；仅匹配有专属类型的行（UserDefined 行不参与，避免误命中）
    inline const DescriptorResourceCatalogEntry *FindResourceCatalogEntryBySSBOType(const SSBOType ssbo_type)
    {
        if (ssbo_type == SSBOType::UserDefined)
            return(nullptr);

        for (const auto &row : kDescriptorResourceCatalog)
            if (row.ssbo_type == ssbo_type)
                return &row;

        return(nullptr);
    }

    /// 按 VAB 语义查找顶点数据行（VertexIndex 无 VAB 语义，不参与）
    inline const DescriptorResourceCatalogEntry *FindVertexCatalogEntryByVABSemantic(const VertexSemantic vab_semantic)
    {
        if (vab_semantic == VertexSemantic::Unknown)
            return(nullptr);

        for (const auto &row : kDescriptorResourceCatalog)
            if (row.cls == ResourceCatalogClass::VertexGeometry
             && row.vab_semantic == vab_semantic)
                return &row;

        return(nullptr);
    }
}//namespace hgl::graph::mtl
