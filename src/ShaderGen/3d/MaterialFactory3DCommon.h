#pragma once

/// 将 StaticMaterialDef + MaterialVariantKey → MaterialCreateInfo* 的公共流程提取为单一入口，
/// 消除各 M_*.cpp 中重复的 registry-lookup → assemble → compile 样板代码。

#include<hgl/mtl/StaticMaterialDef.h>
#include<hgl/mtl/MaterialVariantKey.h>
#include<hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl
{
inline void PopulateVariantKeyVertexAttribBits(MaterialVariantKey &key, const StaticMaterialDef &def)
{
    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
    {
        const FixedVertexEntry &entry = def.vertex_entries[i];

        // Position is NOT a shader-variant VA axis: its presence is universal and its
        // 2D/3D format difference is already encoded in key.position_provider (VAB_Vec2
        // vs DirectVec3).  Including it in va_bits would cause a structural-identity
        // mismatch against registry entries that do not set the Position bit.
        if (entry.attrib == hgl::graph::VertexAttrib::Position)
            continue;

        key.SetVertexAttribEnabled(entry.attrib);
    }
}

namespace contract{struct PhysicalDeviceProfileLite;}
struct Material3DCreateConfig;
class MaterialCreateInfo;

/// 通用 3D 工厂：StaticMaterialDef + VariantKey + VariantDesc → MaterialCreateInfo*
///
/// 内部流程：
///   1. PopulateVariantKeyVertexAttribBits(assemble_key, def)
///   2. AssembleVertexArtifact / AssembleFragmentArtifact（含 SFM 资源收集）
///   3. 合并 VS+FS req_set → MaterialResourceManifest → StaticMaterialDef
///   4. CompileCompositorMaterial(profile, merged_def, vs_glsl, fs_glsl, cfg)
///
/// var_desc 由路由层（MaterialLibrary）传入，无需内部再次 QueryVariant。
///
/// @param debug_tag  用于错误日志的材质标识名（如 "PureColor3D"）
/// @param profile    设备能力 profile
/// @param def        材质定义（descriptor/vertex/MI 元数据）
/// @param var_key    已配置好的材质变体键（用作 assemble_key 基础）
/// @param cfg        运行时配置（可选）
/// @param var_desc   路由层已查找好的 variant 描述符
/// @return           编译好的 MaterialCreateInfo*; 失败返回 nullptr
MaterialCreateInfo *CreateFromFixedDef3D(
    const char *debug_tag,
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const MaterialVariantKey &var_key,
    const Material3DCreateConfig *cfg,
    const MaterialVariantDesc &var_desc);

}//namespace hgl::graph::mtl
