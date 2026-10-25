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
        };

        /// 已实现固定主着色器的领域。新增领域 = 在此加一行 + 提供对应 .frag 文件；
        /// 只加枚举不加文件是**不允许**的（会解析失败，而不是悄悄退化成别的领域）。
        constexpr DomainDirEntry kDomainDirs[] =
        {
            { MaterialDomain::StandardOpaque, "standard_opaque" },
            { MaterialDomain::Sky,            "sky"            },
        };

        constexpr int kDomainDirCount =
            static_cast<int>(sizeof(kDomainDirs) / sizeof(kDomainDirs[0]));

        const char *FindDomainDir(const MaterialDomain domain) noexcept
        {
            for (int i = 0; i < kDomainDirCount; ++i)
                if (kDomainDirs[i].domain == domain)
                    return kDomainDirs[i].dir_name;

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

        const char *dir_name = FindDomainDir(domain);
        if (!dir_name)
            return false;

        const char *stage_name = nullptr;

        if (!IsDomainDepthPurpose(purpose))
        {
            // Sky 是环境 pass 专用领域，没有前向着色形状。
            if (domain == MaterialDomain::Sky)
                return false;

            stage_name = "forward_lit.frag";
        }
        else
        {
            // 天空不进深度/阴影 pass：给它一份深度主着色器只会掩盖调用方错误。
            if (domain == MaterialDomain::Sky)
                return false;

            stage_name = masked_coverage
                       ? "shadow_caster_masked.frag"
                       : "shadow_caster.frag";
        }

        char buffer[128];
        snprintf(buffer, sizeof(buffer), "domain/%s/%s", dir_name, stage_name);

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
