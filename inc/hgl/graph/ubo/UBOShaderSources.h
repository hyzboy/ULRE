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

    constexpr const ShaderBufferSource SBS_SkyInfo =
    {
        DescriptorSetType::Scene,
        "sky",
        "SkyInfo"
    };

    constexpr const ShaderBufferSource SBS_ShadowInfo =
    {
        DescriptorSetType::Scene,
        "shadow",
        "ShadowInfo"
    };
}
