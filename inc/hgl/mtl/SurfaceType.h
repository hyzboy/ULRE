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
        Terrain,
        Sky,

        ENUM_CLASS_RANGE(Unlit, Sky),

        // --- 2D Materials (offset = 20 to leave room for future 3D types) ---
        PureColor2D   = 20,
        PureTexture2D,
        Text2D,
        VertexColor2D,
    };

    constexpr SurfaceType SurfaceType2D_Begin = SurfaceType::PureColor2D;
    constexpr SurfaceType SurfaceType2D_End   = SurfaceType::VertexColor2D;

    constexpr const char* SurfaceTypeNames[] = {
        "Unlit", "Standard", "Skin", "Hair", "Cloth",
        "Eye", "Foliage", "ClearCoat", "Water", "Terrain", "Sky"
    };

    constexpr const char* SurfaceType2DNames[] = {
        "PureColor2D", "PureTexture2D", "Text2D", "VertexColor2D"
    };

    inline const char* GetSurfaceTypeName(SurfaceType st)
    {
        const uint8 idx = static_cast<uint8>(st);
        if (idx >= static_cast<uint8>(SurfaceType::PureColor2D)
         && idx <= static_cast<uint8>(SurfaceType::VertexColor2D))
        {
            return SurfaceType2DNames[idx - static_cast<uint8>(SurfaceType::PureColor2D)];
        }
        if (idx > static_cast<uint8>(SurfaceType::Sky)) return "Unknown";
        return SurfaceTypeNames[idx];
    }

    inline bool Is2DSurfaceType(SurfaceType st)
    {
        const uint8 idx = static_cast<uint8>(st);
        return idx >= static_cast<uint8>(SurfaceType::PureColor2D)
            && idx <= static_cast<uint8>(SurfaceType::VertexColor2D);
    }
}
