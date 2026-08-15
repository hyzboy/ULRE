#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>
#include <cstring>
#include <string>

namespace hgl::graph::mtl
{
    // 逻辑纹理槽位（与具体 descriptor set/binding 解耦）。
    // Resolve 阶段会把这些语义槽映射到 bindless handle + 运行时索引。
    //
    // 名字化约定：snake_case 名字（GetTextureSlotName）是纹理槽的主键，
    // 贯穿 TOML → C++ Recipe/绑定 → GLSL struct 字段名；enum 仅是紧凑的
    // 内部序列化表示（契约层），由名字派生。二者一一对应，不得手写重复表。
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

    // snake_case 名字（TOML 主键 / GLSL 字段名 / C++ 绑定 key）。
    inline const char *GetTextureSlotName(const TextureSlot slot) noexcept
    {
        switch (slot)
        {
        case TextureSlot::BaseColor:   return "base_color";
        case TextureSlot::Normal:      return "normal";
        case TextureSlot::Metallic:    return "metallic";
        case TextureSlot::Roughness:   return "roughness";
        case TextureSlot::Emissive:    return "emissive";
        case TextureSlot::Occlusion:   return "occlusion";
        case TextureSlot::OpacityMask: return "opacity_mask";
        case TextureSlot::Height:      return "height";
        case TextureSlot::Custom0:     return "custom0";
        case TextureSlot::Custom1:     return "custom1";
        }
        return "base_color";
    }

    inline bool ParseTextureSlotName(const char *name, TextureSlot &out) noexcept
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(TextureSlot::RANGE_SIZE); ++i)
        {
            const TextureSlot slot = static_cast<TextureSlot>(i);
            if (std::strcmp(name, GetTextureSlotName(slot)) == 0)
            {
                out = slot;
                return true;
            }
        }
        return false;
    }

    inline bool ParseTextureSlotName(const std::string &name, TextureSlot &out) noexcept
    {
        return ParseTextureSlotName(name.c_str(), out);
    }
}
