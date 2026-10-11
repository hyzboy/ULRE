#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/type/StrChar.h>

namespace hgl::graph
{
    enum class PassType : uint8
    {
        ForwardOpaque = 0,
        ForwardMasked,
        ForwardTransparent,
        ForwardDither,
        ShadowOpaque,
        ShadowMasked,
        EarlyZSolid,
        EarlyZMasked,

        ENUM_CLASS_RANGE(ForwardOpaque, EarlyZMasked)
    };

}
