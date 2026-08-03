#pragma once

#include <hgl/mtl/ShaderBufferSource.h>

namespace hgl::graph::mtl
{
    // UBO resource metadata is kept beside the graph UBO definitions. The
    // legacy mtl/UBOCommon.h header re-exports these symbols for compatibility.
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
