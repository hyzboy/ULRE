#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph::mtl
{
    enum class MaterialPreset : uint8
    {
        VertexColor2D,
        PureColor2D,
        PureTexture2D,
        Text2D,

        PureColor3D,
        VertexColor3D,
        VertexLuminance3D,
        VertexPattleColor3D,
        Gizmo3D,
        TerrainGrid,
        SkyMinimal,
        Billboard2D,
        Standard,
        PBRColor3D,
        VertexLuminance2D,

        ENUM_CLASS_RANGE(VertexColor2D, VertexLuminance2D)
    };
}
