#pragma once

#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class PlatformBackend : uint8
    {
        PC = 0,
        Apple,
        Android,

        ENUM_CLASS_RANGE(PC, Android)
    };

    enum class GeometryFetchMode : uint8
    {
        SSBO = 0,
        VBO,

        ENUM_CLASS_RANGE(SSBO, VBO)
    };
}
