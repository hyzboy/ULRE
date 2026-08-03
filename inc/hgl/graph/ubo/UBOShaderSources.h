#pragma once

#include <hgl/mtl/ShaderBufferSource.h>

namespace hgl::graph::mtl
{
    constexpr const ShaderBufferSource SBS_ViewportInfo =
    {
        DescriptorSetType::Scene,
        "viewport",
        "ViewportInfo"
    };

    constexpr const ShaderBufferSource SBS_CameraInfo =
    {
        DescriptorSetType::Scene,
        "camera",
        "CameraInfo"
    };

    constexpr const ShaderBufferSource SBS_ColorPattle =
    {
        DescriptorSetType::Material,
        "color_pattle",
        "ColorPattle"
    };

    constexpr const ShaderBufferSource SBS_SkyInfo =
    {
        DescriptorSetType::Scene,
        "sky",
        "SkyInfo"
    };
}
