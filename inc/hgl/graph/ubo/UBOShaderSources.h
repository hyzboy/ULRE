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

}
