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
    constexpr const DescriptorResourceCatalogEntry *FindResourceCatalogEntry(const DescriptorSemantic semantic)
    {
        for (const auto &row : kDescriptorResourceCatalog)
            if (row.semantic == semantic)
                return &row;

        return(nullptr);
    }

    /// 按 SSBO 类型查找；仅匹配有专属类型的行（UserDefined 行不参与，避免误命中）
    constexpr const DescriptorResourceCatalogEntry *FindResourceCatalogEntryBySSBOType(const SSBOType ssbo_type)
    {
        if (ssbo_type == SSBOType::UserDefined)
            return(nullptr);

        for (const auto &row : kDescriptorResourceCatalog)
            if (row.ssbo_type == ssbo_type)
                return &row;

        return(nullptr);
    }

    /// 按 VAB 语义查找顶点数据行（VertexIndex 无 VAB 语义，不参与）
    constexpr const DescriptorResourceCatalogEntry *FindVertexCatalogEntryByVABSemantic(const VertexSemantic vab_semantic)
    {
        if (vab_semantic == VertexSemantic::Unknown)
            return(nullptr);

        for (const auto &row : kDescriptorResourceCatalog)
            if (row.cls == ResourceCatalogClass::VertexGeometry
             && row.vab_semantic == vab_semantic)
                return &row;

        return(nullptr);
    }

    // ── 目录自洽性断言（编译期，遍历全表——新增行/新增绑定枚举项自动纳入检查）──
    namespace catalog_check
    {
        constexpr bool StrEqual(const char *a,const char *b) noexcept
        {
            if(!a||!b)return a==b;

            while(*a&&*b)
            {
                if(*a!=*b)return false;
                ++a;++b;
            }

            return *a==*b;
        }

        /// 全表唯一性：语义不重复；同集内固定绑定号不撞号（binding=-1 动态行不参与）；
        /// 固定 SBS 的 buffer 名不重复（复制粘贴错误的主要形态）
        constexpr bool RowsUnique() noexcept
        {
            for(size_t i=0;i<DESCRIPTOR_RESOURCE_CATALOG_COUNT;++i)
                for(size_t j=i+1;j<DESCRIPTOR_RESOURCE_CATALOG_COUNT;++j)
                {
                    const DescriptorResourceCatalogEntry &a=kDescriptorResourceCatalog[i];
                    const DescriptorResourceCatalogEntry &b=kDescriptorResourceCatalog[j];

                    if(a.semantic==b.semantic)return false;

                    if(a.binding>=0&&a.set_type==b.set_type&&a.binding==b.binding)return false;

                    if(a.sbs&&b.sbs&&StrEqual(a.sbs->name,b.sbs->name))return false;
                }

            return true;
        }

        /// Vertex 集完整覆盖：每个 VertexBinding 枚举项恰有一行登记（顺序无关，位图判定）。
        /// 新增 VertexBinding 项却忘记加目录行 → 计数/位图不满 → **编译失败**。
        /// 顶点行同时强制：归 Vertex 集、有固定 SBS、有专属 SSBOType。
        constexpr bool VertexBindingsFullyCovered() noexcept
        {
            constexpr int slot_count=int(VertexBinding::RANGE_SIZE);

            uint32 seen=0;
            int    count=0;

            for(const DescriptorResourceCatalogEntry &row:kDescriptorResourceCatalog)
            {
                if(row.cls!=ResourceCatalogClass::VertexGeometry)continue;

                if(row.set_type!=DescriptorSetType::Vertex)return false;
                if(row.binding<0||row.binding>=slot_count)return false;
                if(row.sbs==nullptr)return false;
                if(row.ssbo_type==SSBOType::UserDefined)return false;

                const uint32 bit=uint32(1)<<row.binding;

                if(seen&bit)return false;

                seen|=bit;
                ++count;
            }

            return count==slot_count
                && seen==((uint32(1)<<slot_count)-uint32(1));
        }

        /// Scene 全局行：必须有固定 SBS 与固定绑定号（全局集无 per-material 动态项）
        constexpr bool SceneRowsWellFormed() noexcept
        {
            for(const DescriptorResourceCatalogEntry &row:kDescriptorResourceCatalog)
            {
                if(row.cls!=ResourceCatalogClass::SceneGlobal)continue;

                if(row.set_type!=DescriptorSetType::Scene)return false;
                if(row.sbs==nullptr)return false;
                if(row.binding<0)return false;
            }

            return true;
        }
    }//namespace catalog_check

    static_assert(catalog_check::RowsUnique(),
                  "资源目录存在重复语义 / 同集绑定号撞号 / 重复 buffer 名");

    static_assert(catalog_check::VertexBindingsFullyCovered(),
                  "VertexBinding 枚举项与目录 VertexGeometry 行未一一对应"
                  "（新增顶点绑定后须在 kDescriptorResourceCatalog 登记一行）");

    static_assert(catalog_check::SceneRowsWellFormed(),
                  "Scene 全局行必须具备固定 SBS 与固定绑定号");
}//namespace hgl::graph::mtl
