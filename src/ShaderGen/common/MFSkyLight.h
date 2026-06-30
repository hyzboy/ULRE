#pragma once

// 统一 SkyLight 资源注入工具。
// GLSL 天光函数实现已迁移到 ShaderLibrary/common/skylight_*.glsl 文件，
// 由 CompositorAssembler 通过 SKYLIGHT_FUNCTION_FILE 标记在合成时选择注入。
// 本文件仅保留 C++ 侧的 descriptor 资源注入辅助功能。

#include<hgl/mtl/SkyLight.h>        // SkyLightAmbientModel
#include<hgl/mtl/StaticMaterialDef.h>

#include <vulkan/vulkan.h>
#include<vector>
#include<cstring>

namespace hgl::graph::mtl {

inline bool SkyCStrEq(const char *lhs,const char *rhs)
{
    return lhs&&rhs&&std::strcmp(lhs,rhs)==0;
}

// ─────────────────────────────────────────────────────────────────────────────
// SkyLight 资源注入结构（可复用到任意材质）
//
// 目标：把“这个 Sky 模式需要追加哪些 descriptor / required_resources”集中在这里。
// 材质工厂只需：
//   1) 取 GetSkyLightResourceInjectionSpec(model)
//   2) 调用 ApplySkyLightResourceInjection(...)
// 即可获得动态资源配置。
// ─────────────────────────────────────────────────────────────────────────────

struct SkyLightResourceInjectionSpec
{
    const char *const *append_fragment_required_resources = nullptr;
    uint32_t append_fragment_required_resource_count = 0;
};

constexpr const char *SKYLIGHT_APPEND_FRAGMENT_RESOURCES_CUBEMAP[] = {
    SKYLIGHT_RESOURCE_KEY_SKY_CUBEMAP,
};

inline SkyLightResourceInjectionSpec GetSkyLightResourceInjectionSpec(const SkyLightAmbientModel model)
{
    const SkyLightDataRequirement req = GetSkyLightDataRequirement(model);

    if (req.need_sky_cubemap)
    {
        return SkyLightResourceInjectionSpec{
            SKYLIGHT_APPEND_FRAGMENT_RESOURCES_CUBEMAP,
            uint32_t(sizeof(SKYLIGHT_APPEND_FRAGMENT_RESOURCES_CUBEMAP) / sizeof(SKYLIGHT_APPEND_FRAGMENT_RESOURCES_CUBEMAP[0]))
        };
    }

    return SkyLightResourceInjectionSpec{};
}

inline void ApplySkyLightResourceInjection(
    const SkyLightResourceInjectionSpec &spec,
    StaticTextureSamplerDescriptors &texture_samplers_io,
    std::vector<const char *> &fragment_resources_io)
{
    (void)texture_samplers_io;
    for (uint32_t i = 0; i < spec.append_fragment_required_resource_count; ++i)
    {
        const char *name = spec.append_fragment_required_resources[i];

        bool exists = false;
        for (const char *cur : fragment_resources_io)
        {
            if (SkyCStrEq(cur, name))
            {
                exists = true;
                break;
            }
        }

        if (!exists)
            fragment_resources_io.emplace_back(name);
    }
}

}//namespace hgl::graph::mtl
