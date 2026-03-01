#pragma once

// 统一 SkyLight 输入与可扩展天光模型选择。
// 约定：所有需要天光输入的材质都通过本块提供的函数读取。
// C++ 排列枚举与映射常量见 SkyLight.h（公共头）。
//
// 设计原则：GLSL 侧不使用 #if/#elif 分支，每个模型提供各自独立的函数实现。
// 合成器（ComposedShaderGenerator）根据 SkyLightAmbientModel 枚举值，
// 选择 GetSkyLightModelImplGLSL(key.ambient) 返回的实现字符串并注入 FS 前部。

#include<hgl/graph/mtl/SkyLight.h>        // SkyLightAmbientModel, SKYLIGHT_GLSL_* 常量
#include<hgl/shadergen/FixedMaterialDef.h>// FixedDescriptorEntry, DescriptorSetType, DescriptorKind
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
    const FixedDescriptorEntry *append_descriptors = nullptr;
    uint32_t append_descriptor_count = 0;

    const char *const *append_fragment_required_resources = nullptr;
    uint32_t append_fragment_required_resource_count = 0;
};

constexpr FixedDescriptorEntry SKYLIGHT_APPEND_DESCRIPTOR_CUBEMAP[] = {
    { DescriptorSetType::Camera, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), SKYLIGHT_RESOURCE_KEY_SKY_CUBEMAP, nullptr, "samplerCube" },
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
            SKYLIGHT_APPEND_DESCRIPTOR_CUBEMAP,
            uint32_t(sizeof(SKYLIGHT_APPEND_DESCRIPTOR_CUBEMAP) / sizeof(SKYLIGHT_APPEND_DESCRIPTOR_CUBEMAP[0])),
            SKYLIGHT_APPEND_FRAGMENT_RESOURCES_CUBEMAP,
            uint32_t(sizeof(SKYLIGHT_APPEND_FRAGMENT_RESOURCES_CUBEMAP) / sizeof(SKYLIGHT_APPEND_FRAGMENT_RESOURCES_CUBEMAP[0]))
        };
    }

    return SkyLightResourceInjectionSpec{};
}

inline void ApplySkyLightResourceInjection(
    const SkyLightResourceInjectionSpec &spec,
    std::vector<FixedDescriptorEntry> &descriptors_io,
    std::vector<const char *> &fragment_resources_io)
{
    for (uint32_t i = 0; i < spec.append_descriptor_count; ++i)
    {
        const auto &entry = spec.append_descriptors[i];

        bool exists = false;
        for (const auto &cur : descriptors_io)
        {
            if (SkyCStrEq(cur.name, entry.name))
            {
                exists = true;
                break;
            }
        }

        if (!exists)
            descriptors_io.emplace_back(entry);
    }

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

// ─────────────────────────────────────────────────────────────────────────────
// GLSL 头部（无分支）— 模型常量 #define + sun 访问宏 + ULRE_GetSkyLightDir()
// 由合成器先于模型实现字符串注入到 FS
// ─────────────────────────────────────────────────────────────────────────────

constexpr const char SKYLIGHT_GLSL_HEADER[] = R"(
// 天光模型常量 — 与 C++ SkyLightAmbientModel 枚举値保持 +1 对应（见 SkyLight.h）
#define ULRE_SKYLIGHT_MODEL_SIMPLE    1
#define ULRE_SKYLIGHT_MODEL_FAKE_ATM  2
#define ULRE_SKYLIGHT_MODEL_CUBEMAP   3
#define ULRE_SKYLIGHT_MODEL_SH        4
#define ULRE_SKYLIGHT_MODEL_IBL       5

#define ULRE_SKY_SUN_DIR    normalize(sky.sun_direction.xyz)
#define ULRE_SKY_SUN_COLOR  (sky.sun_color.rgb * sky.sun_intensity)
#define ULRE_SKY_BASE_COLOR (sky.base_sky_color.rgb)

vec3 ULRE_GetSkyLightDir()
{
    return ULRE_SKY_SUN_DIR;
}
)";

// ─────────────────────────────────────────────────────────────────────────────
// 各模型完整实现 — 每段只定义 ULRE_GetSkyLightColor + ULRE_GetSkyAmbientColor
// 无 #if/#elif 分支，由合成器在编译时根据 SkyLightAmbientModel 选择注入
// 新增模型：在此添加新的 constexpr const char[] + 在 SKYLIGHT_MODEL_IMPL_GLSL[] 中追加即可
// ─────────────────────────────────────────────────────────────────────────────

/// Simple：最基础的仰角渐变 exp2(h)*天空基色，零额外 uniform，最低开销
constexpr const char SKYLIGHT_IMPL_SIMPLE_GLSL[] = R"(
vec3 ULRE_GetSkyLightColor()
{
    return ULRE_SKY_SUN_COLOR;
}
vec3 ULRE_GetSkyAmbientColor()
{
    float h = clamp(ULRE_SKY_SUN_DIR.z * 0.5 + 0.5, 0.0, 1.0);
    return ULRE_SKY_BASE_COLOR * exp2(-(1.0 - h) * 0.8);
}
)";

/// FakeAtmosphere：低层散射近似 + 地平线暖色渐变，无额外 texture，中低配可用
constexpr const char SKYLIGHT_IMPL_FAKE_ATM_GLSL[] = R"(
vec3 ULRE_GetSkyLightColor()
{
    return ULRE_SKY_SUN_COLOR;
}
vec3 ULRE_GetSkyAmbientColor()
{
    float h = clamp(ULRE_SKY_SUN_DIR.z * 0.5 + 0.5, 0.0, 1.0);
    vec3 grad = ULRE_SKY_BASE_COLOR * exp2(-(1.0 - h) * 0.8);
    float horizon = 1.0 - h;
    vec3 warm_tint = mix(vec3(1.0), vec3(1.0, 0.4, 0.05) * 1.2, clamp(horizon, 0.0, 1.0));
    vec3 scatter_mix = mix(grad, warm_tint, 0.5 * sky.sun_intensity);
    float atmosphere = sqrt(max(0.0, 1.0 - h));
    return mix(grad, scatter_mix, atmosphere * 0.7);
}
)";

/// CubeMap：业务模块负责实际 CubeMap 采样（静态或 ComputeShader 大气输出均可）
/// 当前阶段仅提供接口与数据需求占位，具体采样实现留待后续
constexpr const char SKYLIGHT_IMPL_CUBEMAP_GLSL[] =
R"(
// 单 CubeMap 输入（由 EnvironmentSystem 统一配置与绑定）
uniform samplerCube )" SKYLIGHT_RESOURCE_KEY_SKY_CUBEMAP_LITERAL R"(;

vec3 ULRE_GetSkyLightColor()
{
    // TODO(stage-2): 实现真正的太阳方向+CubeMap融合逻辑
    return ULRE_SKY_SUN_COLOR;
}
vec3 ULRE_GetSkyAmbientColor()
{
    // TODO(stage-2): 实现真正的 CubeMap 采样（可按法线/视线/roughness 扩展）
    // 占位返回：保持接口稳定，不在当前阶段引入具体采样策略
    return ULRE_SKY_BASE_COLOR;
}
)";

/// SphericalHarmonics：业务模块负责 SH band 系数求和（无 CubeMap lookup，适合低功耗）
constexpr const char SKYLIGHT_IMPL_SH_GLSL[] = R"(
vec3 ULRE_GetSkyLightColor()
{
    // SH 模式：取太阳色与天空基色的较大値以保证方向光完整性
    return max(ULRE_SKY_SUN_COLOR, ULRE_SKY_BASE_COLOR * 0.5);
}
vec3 ULRE_GetSkyAmbientColor()
{
    // stub：实际 SH 求和（sum Y_l_m * L_l_m）由业务模块完成
    return ULRE_SKY_BASE_COLOR;
}
)";

/// IBL：业务模块负责 CubeMap 采样（多个 IBL 相交由 ComputeShader 混合后输入，此处只取一张）
constexpr const char SKYLIGHT_IMPL_IBL_GLSL[] = R"(
vec3 ULRE_GetSkyLightColor()
{
    return ULRE_SKY_SUN_COLOR;
}
vec3 ULRE_GetSkyAmbientColor()
{
    // stub：实际 IBL CubeMap 采样（textureLod(env_ibl, N, roughness*mip)）由业务模块完成
    return ULRE_SKY_BASE_COLOR;
}
)";

// ─────────────────────────────────────────────────────────────────────────────
// 模型索引表 + 合成器调用的选择函数
// ─────────────────────────────────────────────────────────────────────────────

constexpr const char *const SKYLIGHT_MODEL_IMPL_GLSL[] = {
    SKYLIGHT_IMPL_SIMPLE_GLSL,     // SkyLightAmbientModel::Simple
    SKYLIGHT_IMPL_FAKE_ATM_GLSL,   // SkyLightAmbientModel::FakeAtmosphere
    SKYLIGHT_IMPL_CUBEMAP_GLSL,    // SkyLightAmbientModel::CubeMap
    SKYLIGHT_IMPL_SH_GLSL,         // SkyLightAmbientModel::SphericalHarmonics
    SKYLIGHT_IMPL_IBL_GLSL,        // SkyLightAmbientModel::IBL
};

static_assert(
    sizeof(SKYLIGHT_MODEL_IMPL_GLSL)/sizeof(SKYLIGHT_MODEL_IMPL_GLSL[0])
    == size_t(SkyLightAmbientModel::RANGE_SIZE),
    "SKYLIGHT_MODEL_IMPL_GLSL size mismatch — update when SkyLightAmbientModel enum changes");

/// 合成器调用：根据天光模型枚举值返回对应的 GLSL 实现字符串
/// 注入顺序：SKYLIGHT_GLSL_HEADER → GetSkyLightModelImplGLSL(key.ambient) → fragment_business->code
inline const char *GetSkyLightModelImplGLSL(SkyLightAmbientModel m)
{
    const size_t idx = size_t(m);
    return idx < size_t(SkyLightAmbientModel::RANGE_SIZE)
        ? SKYLIGHT_MODEL_IMPL_GLSL[idx]
        : SKYLIGHT_MODEL_IMPL_GLSL[0];
}

}//namespace hgl::graph::mtl
