#pragma once

#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class QualityTier : uint8
    {
        Lowest = 0,
        Low,
        Medium,
        High,
        Ultra,
        Cinematic,

        ENUM_CLASS_RANGE(Lowest, Cinematic)
    };

    constexpr const char* QualityTierNames[] = {
        "Lowest", "Low", "Medium", "High", "Ultra", "Cinematic"
    };
}
