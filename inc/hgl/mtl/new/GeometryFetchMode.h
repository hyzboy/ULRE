#pragma once

#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class GeometryFetchMode : uint8
    {
        SSBO = 0,
        VBO,

        ENUM_CLASS_RANGE(SSBO, VBO)
    };
}
