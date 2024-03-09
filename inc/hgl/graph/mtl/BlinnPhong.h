#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/ShaderBuffer.h>
#include<hgl/math/Vector.h>
STD_MTL_NAMESPACE_BEGIN
namespace blinnphong
{
    struct SunLight
    {
        Vector4f direction;
        Vector4f color;
    };//struct SunLight

    constexpr const ShaderBufferSource SBS_SunLight=
    {
        "SunLight",
        "sun",

        R"(
        vec4 direction;
        vec4 color;
)"
    };
}//namespace blinnphong
STD_MTL_NAMESPACE_END
