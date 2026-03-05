#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class PrimitiveType:uint32
    {
        Points=0,
        Lines,
        LineStrip,
        Triangles,
        TriangleStrip,
        Fan,
        LinesAdj,
        LineStripAdj,
        TrianglesAdj,
        TriangleStripAdj,
        Patchs,

        SolidRectangles=0x100,
        SolidCircles,

        WireRectangles=0x200,
        WireCircles,

        Billboard=0x500,

        OBB=0x600,

        ENUM_CLASS_RANGE(Points,Patchs),

        Error
    };

    const char *GetPrimName(const PrimitiveType &prim);
    const PrimitiveType ParsePrimitiveType(const char *name,int len=0);

    bool CheckGeometryShaderIn(const PrimitiveType &);
    bool CheckGeometryShaderOut(const PrimitiveType &);
}
