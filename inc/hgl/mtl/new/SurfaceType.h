#pragma once

#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class SurfaceType : uint8
    {
        Unlit = 0,
        Standard,
        Skin,
        Hair,
        Cloth,
        Eye,
        Foliage,
        ClearCoat,
        Water,
        Sky,

        ENUM_CLASS_RANGE(Unlit, Sky)
    };

    constexpr const char* SurfaceTypeNames[] = {
        "Unlit", "Standard", "Skin", "Hair", "Cloth",
        "Eye", "Foliage", "ClearCoat", "Water", "Sky"
    };

    inline const char* GetSurfaceTypeName(SurfaceType st)
    {
        const uint8 idx = static_cast<uint8>(st);
        if (idx > static_cast<uint8>(SurfaceType::Sky)) return "Unknown";
        return SurfaceTypeNames[idx];
    }
}
