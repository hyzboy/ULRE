#include <hgl/mtl/DomainShaderTemplate.h>

#include <cstdio>

namespace hgl::graph::mtl
{
    namespace
    {
        struct DomainDirEntry
        {
            MaterialDomain domain;
            const char *dir_name;       ///< ShaderLibrary/domain/ 下的子目录名
            const char *forward_file;   ///< 前向 pass 的主着色器文件名
            bool supports_depth;        ///< 是否参与深度/阴影 pass
        };

        /// 已实现固定主着色器的领域。新增领域 = 在此加一行 + 提供对应 .frag 文件；
        /// 只加枚举不加文件是**不允许**的（会解析失败，而不是悄悄退化成别的领域）。
        ///
        /// 前向文件名**逐领域给出**，不共用一套命名：StandardOpaque 的主着色器是
        /// 一份完整的 PBR 前向着色（`forward_lit.frag`），而 Sky 是自己的一套环境
        /// 采样（`sky.frag`，输出方向向量而非世界坐标）。用同一个文件名去拼会让
        /// 后加的领域在编译期就找不到文件。
        ///
        /// 一个领域**允许**指向另一个领域的目录与文件（VertexPalette 复用
        /// vertex_color）：两者片元阶段逐字节相同，差异只在顶点语义，而顶点语义
        /// 由 `DomainContracts[]` 驱动顶点阶段，与本表无关。
        constexpr DomainDirEntry kDomainDirs[] =
        {
            { MaterialDomain::StandardOpaque,    "standard_opaque",    "forward_lit.frag", true  },
            { MaterialDomain::Sky,               "sky",                "sky.frag",         false },
            { MaterialDomain::UnlitTexture,      "unlit_texture",      "unlit_texture.frag",      false },
            { MaterialDomain::VertexColor,       "vertex_color",       "vertex_color.frag",       false },
            { MaterialDomain::VertexPalette,     "vertex_color",       "vertex_color.frag",       false },
            { MaterialDomain::VertexLuminance,   "vertex_luminance",   "vertex_luminance.frag",   false },
            { MaterialDomain::UnlitSolid,        "unlit_solid",        "unlit_solid.frag",        false },
            { MaterialDomain::Checker2D,         "checker_2d",         "checker_2d.frag",         false },
            { MaterialDomain::Checker3D,         "checker_3d",         "checker_3d.frag",         false },
            { MaterialDomain::ErrorChecker3D,    "error_checker_3d",   "error_checker_3d.frag",   false },
            { MaterialDomain::Text2D,            "text_2d",            "text_2d.frag",            false },
            { MaterialDomain::DebugNormal,       "debug_normal",       "debug_normal.frag",       false },
        };

        constexpr int kDomainDirCount =
            static_cast<int>(sizeof(kDomainDirs) / sizeof(kDomainDirs[0]));

        const DomainDirEntry *FindDomainEntry(const MaterialDomain domain) noexcept
        {
            for (int i = 0; i < kDomainDirCount; ++i)
                if (kDomainDirs[i].domain == domain)
                    return &kDomainDirs[i];

            return nullptr;
        }
    }//namespace

    bool ResolveDomainShaderTemplate(
        const MaterialDomain domain,
        const ShaderProgramPurpose purpose,
        const bool masked_coverage,
        DomainShaderTemplate &out_template) noexcept
    {
        out_template.domain = MaterialDomain::Custom;
        out_template.relative_path.Clear();

        const DomainDirEntry *entry = FindDomainEntry(domain);
        if (!entry)
            return false;

        const char *stage_name = nullptr;

        if (!IsDomainDepthPurpose(purpose))
        {
            // 前向 pass：文件名取自表，不做推断。
            stage_name = entry->forward_file;
        }
        else
        {
            // 天空不进深度/阴影 pass：给它一份深度主着色器只会掩盖调用方错误。
            if (!entry->supports_depth)
                return false;

            stage_name = masked_coverage
                       ? "shadow_caster_masked.frag"
                       : "shadow_caster.frag";
        }

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "domain/%s/%s", entry->dir_name, stage_name);

        out_template.domain = domain;
        out_template.relative_path = buffer;
        return true;
    }

    int GetDomainShaderTemplateCount() noexcept
    {
        return kDomainDirCount;
    }

    MaterialDomain GetDomainShaderTemplateDomainAt(const int index) noexcept
    {
        if (index < 0 || index >= kDomainDirCount)
            return MaterialDomain::Custom;

        return kDomainDirs[index].domain;
    }
}//namespace hgl::graph::mtl
