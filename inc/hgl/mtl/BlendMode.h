#pragma once

#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class BlendMode : uint8
    {
        Opaque = 0,
        Masked,
        Transparent,
        Dither,

        ENUM_CLASS_RANGE(Opaque, Dither)
    };
}
