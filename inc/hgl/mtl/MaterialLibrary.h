#pragma once

#include<hgl/vk/VK.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<hgl/mtl/MaterialBuildFlags.h>
#include<hgl/mtl/MaterialPreset.h>
#include<hgl/mtl/MaterialVariantKey.h>
#include<vector>
#include<string>

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

// Phase-A migration helpers: preset <-> variant mapping.
SurfaceType MapPresetToSurfaceType(MaterialPreset preset);
MaterialVariantKey MapPresetToVariantKey(const MaterialPreset mtl_id);

/// Apply per-cfg overrides (geometry_mode, texture_source_bits, sampler_feature_bits) to an
/// already-constructed MaterialVariantKey.  Used by both CreateMaterialCreateInfo and
/// MaterialManager::CreateMaterial so the same logic is executed regardless of which entry
/// point the application uses.
void ApplyCreateConfigToVariantKey(MaterialVariantKey &key, const MaterialCreateConfig *cfg);

}//namespace hgl::graph::mtl

