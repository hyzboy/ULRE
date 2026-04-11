#pragma once

#include <hgl/type/EnumUtil.h>

namespace hgl::graph::mtl
{
    // Internal shader routing classification.
    // Semantic/content-facing code should use MaterialPreset.
    enum class MaterialSurfaceClass : uint8
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
        Terrain,
        Sky,

        ENUM_CLASS_RANGE(Unlit, Sky),
    };

    constexpr const char *MaterialSurfaceClassNames[] = {
        "Unlit", "Standard", "Skin", "Hair", "Cloth",
        "Eye", "Foliage", "ClearCoat", "Water", "Terrain", "Sky"
    };

    inline const char *GetMaterialSurfaceClassName(const MaterialSurfaceClass c)
    {
        const uint8 idx = static_cast<uint8>(c);
        if (idx > static_cast<uint8>(MaterialSurfaceClass::Sky))
            return "Unknown";

        return MaterialSurfaceClassNames[idx];
    }
}
