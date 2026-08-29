#pragma once

#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class SurfaceType : uint8
    {
        Unlit = 0,
        Lit,
        Sky,   // Sky 有真特化（sky_minimal_surface）——B8 清理保留

        ENUM_CLASS_RANGE(Unlit, Sky)
    };

    constexpr const char* SurfaceTypeNames[] = {
        "Unlit", "Lit", "Sky"
    };

    inline const char* GetSurfaceTypeName(SurfaceType st)
    {
        const uint8 idx = static_cast<uint8>(st);
        if (idx > static_cast<uint8>(SurfaceType::Sky)) return "Unknown";
        return SurfaceTypeNames[idx];
    }
}
