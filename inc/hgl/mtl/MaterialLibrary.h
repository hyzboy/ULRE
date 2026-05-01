#pragma once

#include<hgl/vk/VK.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<hgl/mtl/MaterialBuildFlags.h>
#include<hgl/mtl/MaterialPreset.h>
#include<hgl/mtl/MaterialVariantKey.h>
#include<vector>
#include<string>
#include<optional>

namespace hgl::graph::mtl{

struct MaterialCreateConfig;
class MaterialCreateInfo;

/// 仅声明材质创建函数，不产生任何注册或全局常量副作用。
#define DECLARE_MATERIAL_CREATOR(name,cfg_type) \
MaterialCreateInfo *Create##name(const contract::PhysicalDeviceProfileLite *profile,cfg_type *); \
\
inline MaterialCreateInfo *Create##name(const contract::PhysicalDeviceProfileLite *profile)  \
{   \
    cfg_type cfg;   \
    return Create##name(profile,&cfg);  \
}

// Semantic entry path: MaterialPreset is authoring/content intent. The library resolves it through
// runtime material LOD and then maps it to a concrete MaterialVariantKey / shading implementation.
MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialPreset mtl_id,
                                             MaterialCreateConfig *cfg);

// Low-level entry path: callers already know the exact variant/shader-facing routing key.
MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             MaterialCreateConfig *cfg);

// Bootstrap-only global fallback. This exists so the current program can run with a single built-in
// material implementation level. It should not be treated as the final architecture for forward,
// clustered, or VBuffer/deferred paths where material LOD may be chosen per object, per tile, or
// even per pixel.
MaterialLOD GetDefaultMaterialLOD();

// Resolve a semantic preset to the preset family used by the current material implementation level.
// At this stage semantic presets still collapse to Standard at MaterialLOD::Base.
MaterialPreset ResolveMaterialPresetForLOD(MaterialPreset preset,
                                           MaterialLOD lod = MaterialLOD::Base);

const char *GetMaterialPresetName(const MaterialPreset mtl_id);

// 启动期自检：校验内置变体模板是否可组装，失败诊断写入 diagnostics。
bool ValidateBuiltinMaterialVariants(const std::string &shader_library_path,
                                     std::vector<std::string> &diagnostics);

// 导出内置变体快照文本，用于回归对比。
std::string GetBuiltinMaterialVariantSnapshot();

// ---------------------------------------------------------------------------
// [Step 3.5 T1] Variant Key 单轨化路由入口
// ---------------------------------------------------------------------------
// RuntimeKeyOverrides 描述「在 preset 模板基础上，业务层运行期可以覆盖的字段」。
// 任何未设置（std::nullopt / 0 / false）的字段一律沿用 preset 模板的默认值。
//
// 设计准则：
//   * preset 决定 surface/geom/lighting/默认 vertex_attribs 等结构性字段；
//   * RuntimeKeyOverrides 仅承载与单一实例渲染相关的运行期决策；
//   * 任何 caller 不得绕过 RouteKey 直接拼装 MaterialVariantKey。
struct RuntimeKeyOverrides
{
    std::optional<PositionProviderId> position_provider;     // Step 11.D: new canonical provider field
    std::optional<RenderAlphaMode>   blend_mode;             // 同上
    std::optional<PassType>          pass_hint;              // 同上
    std::optional<SkyLightAmbientModel> sky_ambient_model;   // 同上
    std::optional<LightingModel>     lighting_model;         // 同上
    uint32                           extra_vertex_attrib_bits = 0;  // 与 preset 默认 OR 合并
    bool                             debug_shading = false;          // 仅置位
};

/// [Step 3.5 T1] **唯一**的 MaterialVariantKey 构造入口。
/// preset             —— 业务/内容意图（已经过 MaterialLOD 解析的 canonical preset）。
/// extra_attrib_bits  —— 额外要叠加的顶点属性位（位掩码 = OR(VertexAttribFeatureBit(...)),
///                       通常由 caller 已知的几何属性决定）。0 表示沿用 preset 默认。
/// ov                 —— 业务层允许覆盖的运行期字段集合。
MaterialVariantKey RouteKey(MaterialPreset preset,
                            uint32 extra_attrib_bits,
                            const RuntimeKeyOverrides &ov) noexcept;

inline MaterialVariantKey RouteKey(MaterialPreset preset) noexcept
{
    return RouteKey(preset, 0u, RuntimeKeyOverrides{});
}

// Phase-A migration helpers: preset <-> variant mapping.
// [Step 3.5 T1] DEPRECATED：转发到 RouteKey()，T2/T3 完成后将彻底删除。
[[deprecated("use RouteKey(preset, extra_attrib_bits, ov) -- see VertexInputFormat_plan.md Step 3.5 T1")]]
MaterialVariantKey MapPresetToVariantKey(const MaterialPreset mtl_id);

/// Apply per-cfg overrides (geometry_mode, texture_source_bits, sampler_feature_bits) to an
/// already-constructed MaterialVariantKey.  Used by both CreateMaterialCreateInfo and
/// ShaderMaterialProgramManager::CreateMaterial so the same logic is executed regardless of which entry
/// point the application uses.
void ApplyCreateConfigToVariantKey(MaterialVariantKey &key, const MaterialCreateConfig *cfg);

}//namespace hgl::graph::mtl

