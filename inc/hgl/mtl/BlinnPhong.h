#pragma once

#include<hgl/mtl/StdMaterial.h>
#include<hgl/mtl/ShaderBufferSource.h>
#include<hgl/math/Vector.h>
namespace hgl::graph::mtl{
namespace blinnphong
{
    struct SunLight
    {
        math::Vector4f direction;
        math::Vector4f color;
    };//struct SunLight

    constexpr const ShaderBufferSource SBS_SunLight=
    {
        DescriptorSetType::Scene,

        "sun",

        "SunLight"
    };
}//namespace blinnphong
}//namespace hgl::graph::mtl
