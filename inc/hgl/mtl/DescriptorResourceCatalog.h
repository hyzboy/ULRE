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
    /// 全局集按帧绑定（per-material 侧跳过）。
    /// （Vertex/PerObject/Material 集已随 BDA 化整体退场——顶点流经 MeshDrawParams
    /// 行内基址、行表经 pc_root 地址、材质行经地址行表到达 shader，均无描述符。
    /// 契约恒 Scene UBO 条目，目录只余 SceneGlobal 行。）
    enum class ResourceCatalogClass : uint8
    {
        SceneGlobal,     ///< Scene 集全局 UBO（P1 已全局化——per-material 注册跳过）
    };

    /// 描述符资源目录——"语义 → 集合/绑定/SBS"的唯一真源。
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
        bool engine_builtin;             ///< 能力子集校验：无条件内置允许（false=有条件规则或需声明）
    };

    /// 与各枚举行数对齐的资源目录（行序即发现序，查找线性、语义唯一）。
    constexpr const DescriptorResourceCatalogEntry kDescriptorResourceCatalog[]=
    {
        // ── SceneGlobal：全局 UBO（一帧写/绑一次；binding=SceneBinding 枚举）──
        { DescriptorSemantic::ViewportInfo,         ResourceCatalogClass::SceneGlobal, &SBS_ViewportInfo,  DescriptorSetType::Scene,    int(SceneBinding::Viewport),      SSBOType::UserDefined, true  },
        { DescriptorSemantic::CameraInfo,           ResourceCatalogClass::SceneGlobal, &SBS_CameraInfo,    DescriptorSetType::Scene,    int(SceneBinding::Camera),        SSBOType::UserDefined, false },
        { DescriptorSemantic::SkyInfo,              ResourceCatalogClass::SceneGlobal, &SBS_SkyInfo,       DescriptorSetType::Scene,    int(SceneBinding::Sky),           SSBOType::UserDefined, false },
        { DescriptorSemantic::MaterialColorPalette, ResourceCatalogClass::SceneGlobal, &SBS_ColorPalette,  DescriptorSetType::Scene,    int(SceneBinding::ColorPalette),  SSBOType::UserDefined, false },

        // ── PerDraw/PerObject 行已删（A6-2b-b2：L2W/L2WIndex/MeshDrawParams/
        // MaterialPrivateDataIndex 全 BDA——无描述符无目录行；数据槽行表需求由
        // schema.requires_runtime_data_rows 编译期直判承载，不经目录）。
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

        /// Scene 集完整覆盖：每个 SceneBinding 枚举项恰有一行登记（位图判定）。
        /// 新增 SceneBinding 项却忘记加目录行 → 计数/位图不满 → **编译失败**。
        constexpr bool SceneBindingsFullyCovered() noexcept
        {
            constexpr int slot_count=int(SceneBinding::RANGE_SIZE);

            uint32 seen=0;
            int    count=0;

            for(const DescriptorResourceCatalogEntry &row:kDescriptorResourceCatalog)
            {
                if(row.cls!=ResourceCatalogClass::SceneGlobal)continue;

                if(row.binding<0||row.binding>=slot_count)return false;

                const uint32 bit=uint32(1)<<row.binding;

                if(seen&bit)return false;

                seen|=bit;
                ++count;
            }

            return count==slot_count
                && seen==((uint32(1)<<slot_count)-uint32(1));
        }
    }//namespace catalog_check

    static_assert(catalog_check::RowsUnique(),
                  "资源目录存在重复语义 / 同集绑定号撞号 / 重复 buffer 名");

    static_assert(catalog_check::SceneRowsWellFormed(),
                  "Scene 全局行必须具备固定 SBS 与固定绑定号");

    static_assert(catalog_check::SceneBindingsFullyCovered(),
                  "SceneBinding 枚举项与目录 SceneGlobal 行未一一对应"
                  "（新增 Scene 绑定后须在 kDescriptorResourceCatalog 登记一行）");
}//namespace hgl::graph::mtl
