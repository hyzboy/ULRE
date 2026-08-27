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

        ENUM_CLASS_RANGE(Points,Fan),

        Error
    };
}
