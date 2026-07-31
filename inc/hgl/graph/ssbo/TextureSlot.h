#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>

namespace hgl::graph::mtl
{
    // 逻辑纹理槽位（与具体 descriptor set/binding 解耦）。
    // Resolve 阶段会把这些语义槽映射到 bindless handle + 运行时索引。
    enum class TextureSlot : uint8_t
    {
        BaseColor = 0,
        Normal,
        Metallic,
        Roughness,
        Emissive,
        Occlusion,
        OpacityMask,
        Height,
        Custom0,
        Custom1,

        ENUM_CLASS_RANGE(BaseColor, Custom1)
    };
}
