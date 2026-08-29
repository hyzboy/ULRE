#pragma once

#include <hgl/graph/ShaderBufferSource.h>

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

    constexpr const ShaderBufferSource SBS_ColorPalette =
    {
        DescriptorSetType::Scene,
        "color_palette",
        "ColorPalette"
    };

    constexpr const ShaderBufferSource SBS_SkyInfo =
    {
        DescriptorSetType::Scene,
        "sky",
        "SkyInfo"
    };
}
