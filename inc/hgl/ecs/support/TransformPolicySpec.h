#pragma once

#include <cstdint>

namespace hgl::ecs
{
    // Camera-facing is independent from screen-space scaling.
    enum class FacingPolicy : uint8_t
    {
        None = 0,
        CameraFacing,
        AxisLocked,
    };

    struct TransformPolicySpec
    {
        FacingPolicy facing = FacingPolicy::None;
        bool fixedScreenSpaceScale = false;
    };
}
