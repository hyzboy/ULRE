#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>
#include<hgl/math/Vector.h>
STD_MTL_NAMESPACE_BEGIN
namespace blinnphong
{
    struct SunLight
    {
        math::Vector4f direction;
        math::Vector4f color;
    };//struct SunLight

    constexpr const ShaderBufferSource SBS_SunLight=
    {
        DescriptorSetType::World,

        "sun",

        "SunLight",

        R"(
        vec4 direction;
        vec4 color;
)"
    };
}//namespace blinnphong
STD_MTL_NAMESPACE_END
