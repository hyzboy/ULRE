#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/EnumUtil.h>
#include <hgl/mtl/PassType.h>

namespace hgl::graph::mtl
{
    enum class RenderPhase : uint8
    {
        Forward = 0,
        Shadow,
        EarlyZ,

        ENUM_CLASS_RANGE(Forward, EarlyZ)
    };

    constexpr RenderPhase ToRenderPhase(const PassType pass) noexcept
    {
        switch (pass)
        {
        case PassType::ShadowOpaque:
        case PassType::ShadowMasked:
            return RenderPhase::Shadow;

        case PassType::EarlyZSolid:
        case PassType::EarlyZMasked:
            return RenderPhase::EarlyZ;

        default:
            return RenderPhase::Forward;
        }
    }
}
