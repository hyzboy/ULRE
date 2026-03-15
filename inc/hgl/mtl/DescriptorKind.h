#pragma once

#include<hgl/CoreType.h>

namespace hgl::graph::mtl
{
    enum class DescriptorKind : uint8
    {
        UBO,
        SSBO,
        Texture,
        TextureSampler,
    };
}//namespace hgl::graph::mtl