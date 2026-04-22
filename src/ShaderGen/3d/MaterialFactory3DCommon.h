#pragma once

/// 将 StaticMaterialDef + MaterialVariantKey → MaterialCreateInfo* 的公共流程提取为单一入口，
/// 消除各 M_*.cpp 中重复的 registry-lookup → assemble → compile 样板代码。

#include<hgl/mtl/StaticMaterialDef.h>
#include<hgl/mtl/MaterialVariantKey.h>

namespace hgl::graph::mtl{

inline void PopulateVariantKeyVertexAttribBits(MaterialVariantKey &key, const StaticMaterialDef &def)
{
    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
    {
        const FixedVertexEntry &entry = def.vertex_entries[i];
        key.SetVertexAttribEnabled(entry.attrib);

        // Detect vec2 Position to distinguish VertexLuminance2D (vec2) from VertexLuminance3D (vec3).
        if (entry.attrib == hgl::graph::VertexAttrib::Position && entry.type == hgl::graph::VAT_VEC2)
            key.SetVec2Position(true);
    }
}

namespace contract{struct PhysicalDeviceProfileLite;}
struct Material3DCreateConfig;
class MaterialCreateInfo;

/// 通用 3D 工厂：StaticMaterialDef + VariantKey → MaterialCreateInfo*
///
/// 内部流程：
///   1. GetBuiltinVariantRegistry().QueryVariant(var_key)
///   2. CompositorAssembler::Assemble(var_key, *var_desc)
///   3. CompileCompositorMaterial(profile, def, vs_glsl, fs_glsl, cfg)
///
/// @param debug_tag  用于错误日志的材质标识名（如 "PureColor3D"）
/// @param profile    设备能力 profile
/// @param def        材质定义（descriptor/vertex/MI 元数据）
/// @param var_key    已配置好的材质变体键
/// @param cfg        运行时配置（可选）
/// @return           编译好的 MaterialCreateInfo*; 失败返回 nullptr
MaterialCreateInfo *CreateFromFixedDef3D(
    const char *debug_tag,
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const MaterialVariantKey &var_key,
    const Material3DCreateConfig *cfg);

}//namespace hgl::graph::mtl
