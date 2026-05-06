#pragma once

/// 将 StaticMaterialDef + MaterialVariantKey → MaterialCreateInfo* 的公共流程提取为单一入口，
/// 消除各 M_*.cpp 中重复的 registry-lookup → assemble → compile 样板代码。

#include<hgl/mtl/StaticMaterialDef.h>
#include<hgl/mtl/MaterialVariantKey.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include <memory>

namespace hgl::graph::mtl{

inline void PopulateVariantKeyVertexAttribBits(MaterialVariantKey &key, const StaticMaterialDef &def)
{
    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
    {
        const FixedVertexEntry &entry = def.vertex_entries[i];
        key.SetVertexAttribEnabled(entry.attrib);

        // Detect vec2 Position to distinguish VertexLuminance2D (vec2) from VertexLuminance3D (vec3).
        if (entry.attrib == hgl::graph::VertexAttrib::Position && entry.type == hgl::graph::VAT_VEC2)
        {
            key.position_provider = PositionProviderId::VAB_Vec2;
        }
    }
}

namespace contract{struct PhysicalDeviceProfileLite;}
struct Material3DCreateConfig;
class MaterialCreateInfo;

std::unique_ptr<MaterialCreateInfo> CreateFromFixedDef3DOwned(
    const char *debug_tag,
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const MaterialVariantKey &var_key,
    const Material3DCreateConfig *cfg,
    const MaterialVariantDesc &var_desc);

}//namespace hgl::graph::mtl
