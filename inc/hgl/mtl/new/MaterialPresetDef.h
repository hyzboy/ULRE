#pragma once

#include "SurfaceType.h"
#include "QualityTier.h"
#include "MaterialCategory.h"
#include <hgl/type/String.h>

namespace hgl::graph
{
    struct TextureSlotDef
    {
        AnsiString name;            // "albedo", "normal", "metallic_roughness", ...
        QualityTier min_tier;       // 低于此档位不绑定此槽（使用默认纹理）
        bool required;              // 是否必须
    };

    struct MaterialPresetDef
    {
        uint16          preset_id;
        SurfaceType     surface_type;
        MaterialCategory category;
        AnsiString      name;               // "StandardTexture", "PureColor2D", ...
        AnsiString      mi_struct_name;     // GLSL 中 MI 结构体名
        uint32          mi_struct_size;     // MI 数据字节数
        TextureSlotDef  texture_slots[8];   // 最多 8 个纹理槽
        uint8           texture_slot_count;
        SurfaceType     fallback_surface_type;      // MaterialProgram LOD 降级目标
        QualityTier     unique_feature_min_tier;    // 低于此档位 fallback 到 fallback_surface_type
    };
}
