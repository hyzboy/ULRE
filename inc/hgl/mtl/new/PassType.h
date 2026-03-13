#pragma once

#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class PassType : uint8
    {
        ForwardOpaque = 0,
        ForwardMasked,
        ForwardTransparent,
        ForwardDither,
        ForwardA2C,
        ShadowOpaque,
        ShadowMasked,
        EarlyZSolid,
        EarlyZMasked,
        VBufferID,

        ENUM_CLASS_RANGE(ForwardOpaque, VBufferID)
    };
}
