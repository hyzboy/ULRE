#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/vk/VKFormat.h>

namespace hgl::graph
{
    /// Describes the dimensionality / source of the Position vertex attribute.
    enum class PositionType : uint8
    {
        None = 0,   ///< No position input (e.g. purely procedural / PCG)
        Vec2,       ///< 2-component position (x, y)
        Vec3,       ///< 3-component position (x, y, z); passed through as-is

        ENUM_CLASS_RANGE(None, Vec3)
    };

    inline VkFormat PositionTypeToVkFormat(const PositionType t) noexcept
    {
        switch (t)
        {
        case PositionType::Vec2: return VF_V2F;
        case PositionType::Vec3: return VF_V3F;
        default:                 return VK_FORMAT_UNDEFINED;
        }
    }

    inline uint32 PositionTypeStride(const PositionType t) noexcept
    {
        switch (t)
        {
        case PositionType::Vec2: return sizeof(float) * 2;
        case PositionType::Vec3: return sizeof(float) * 3;
        default:                 return 0;
        }
    }
}//namespace hgl::graph
