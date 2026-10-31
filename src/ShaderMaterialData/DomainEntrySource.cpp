#include <hgl/mtl/DomainEntrySource.h>

#include <cstdio>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        struct DomainDirEntry
        {
            MaterialDomain domain;
            const char *dir_name;       ///< ShaderLibrary/domain/ 下的子目录名
            const char *forward_file;   ///< 前向 pass 的主着色器文件名
            bool supports_depth_purpose;        ///< 是否参与深度/阴影 pass
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
        constexpr DomainDirEntry kDomainEntrySources[] =
        {
            { MaterialDomain::StandardOpaque,    "standard_opaque",    "forward_lit.frag", true  },
            { MaterialDomain::Sky,               "sky",                "sky.frag",         false },
            { MaterialDomain::UnlitTexture,      "unlit_texture",      "unlit_texture.frag",      false },
            // 世界空间无光贴面：片元阶段与 UnlitTexture 逐字节相同，直接复用同一
            // 目录与同一份固定主着色器（本表允许跨领域复用），差异全部在顶点侧，
            // 而顶点侧由 DomainContracts[] 的规范顶点通路驱动，与本表无关。
            { MaterialDomain::UnlitTexture3D,    "unlit_texture",      "unlit_texture.frag",      false },
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
            static_cast<int>(sizeof(kDomainEntrySources) / sizeof(kDomainEntrySources[0]));

        /// 「领域 × 表面形态 → 主着色器文件」。
        ///
        /// 同一领域的片元阶段可以有多种**表面形态**（目前只有 Sky：解析式渐变 /
        /// 立方图），配方用 `[fragment].surface_module_name` 选择，空 = 领域默认形态。
        ///
        /// 为什么不写成 `if (domain == Sky)` 分支：形态是三元映射，写成领域分支后
        /// 新增第二种形态必须回来改路由代码，而且 `ParseFragment` 的合法性判定与
        /// 这里的选择逻辑会各自演化（同一事实两份判断，早晚不一致）。
        struct DomainSurfaceForm
        {
            MaterialDomain domain;
            const char *surface_module_name;   ///< nullptr = 该领域的默认形态
            const char *file_name;             ///< ShaderLibrary/domain/<dir>/ 下的文件名
        };

        constexpr DomainSurfaceForm kDomainSurfaceForms[] =
        {
            { MaterialDomain::Sky, nullptr,               "sky_minimal.frag" },
            { MaterialDomain::Sky, "surface_sky_minimal", "sky_minimal.frag" },
            { MaterialDomain::Sky, "surface_sky_cube",    "sky.frag"         },
        };

        constexpr int kDomainSurfaceFormCount =
            static_cast<int>(sizeof(kDomainSurfaceForms) / sizeof(kDomainSurfaceForms[0]));

        /// 该领域是否登记了表面形态表。登记了就必须按表选，表外模块名是错误，
        /// 而不是「随便退化到默认形态」。
        bool DomainHasSurfaceForms(const MaterialDomain domain) noexcept
        {
            for (int i = 0; i < kDomainSurfaceFormCount; ++i)
                if (kDomainSurfaceForms[i].domain == domain)
                    return true;

            return false;
        }

        const char *FindDomainSurfaceFormFile(
            const MaterialDomain domain,
            const char *surface_module_name) noexcept
        {
            const bool default_form = !surface_module_name || !surface_module_name[0];

            for (int i = 0; i < kDomainSurfaceFormCount; ++i)
            {
                const DomainSurfaceForm &form = kDomainSurfaceForms[i];
                if (form.domain != domain)
                    continue;

                if (default_form)
                {
                    if (!form.surface_module_name)
                        return form.file_name;
                }
                else if (form.surface_module_name
                      && std::strcmp(form.surface_module_name, surface_module_name) == 0)
                {
                    return form.file_name;
                }
            }

            return nullptr;
        }

        const DomainDirEntry *FindDomainEntry(const MaterialDomain domain) noexcept
        {
            for (int i = 0; i < kDomainDirCount; ++i)
                if (kDomainEntrySources[i].domain == domain)
                    return &kDomainEntrySources[i];

            return nullptr;
        }
    }//namespace

    bool ResolveDomainEntrySource(
        const MaterialDomain domain,
        const ShaderProgramPurpose purpose,
        const bool masked_coverage,
        DomainEntrySource &out_entry_source,
        const char *surface_module_name) noexcept
    {
        out_entry_source.domain = MaterialDomain::Custom;
        out_entry_source.relative_path.Clear();

        const DomainDirEntry *entry = FindDomainEntry(domain);
        if (!entry)
            return false;

        const char *stage_name = nullptr;

        if (!IsDomainDepthPurpose(purpose))
        {
            if (DomainHasSurfaceForms(domain))
            {
                // 有形态表：文件名只能由表给出，表外模块名 ⇒ 失败。
                stage_name = FindDomainSurfaceFormFile(domain, surface_module_name);
                if (!stage_name)
                    return false;
            }
            else
            {
                // 无形态表：只有一张固定主着色器，不接受配方自选模块名。
                if (surface_module_name && surface_module_name[0])
                    return false;

                stage_name = entry->forward_file;
            }
        }
        else
        {
            // 天空不进深度/阴影 pass：给它一份深度主着色器只会掩盖调用方错误。
            if (!entry->supports_depth_purpose)
                return false;

            stage_name = masked_coverage
                       ? "shadow_caster_masked.frag"
                       : "shadow_caster.frag";
        }

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "domain/%s/%s", entry->dir_name, stage_name);

        out_entry_source.domain = domain;
        out_entry_source.relative_path = buffer;
        return true;
    }

    int GetDomainEntrySourceCount() noexcept
    {
        return kDomainDirCount;
    }

    MaterialDomain GetDomainEntrySourceDomainAt(const int index) noexcept
    {
        if (index < 0 || index >= kDomainDirCount)
            return MaterialDomain::Custom;

        return kDomainEntrySources[index].domain;
    }

    bool HasMaterialDomainEntrySource(const MaterialDomain domain) noexcept
    {
        return FindDomainEntry(domain) != nullptr;
    }
}//namespace hgl::graph::mtl
