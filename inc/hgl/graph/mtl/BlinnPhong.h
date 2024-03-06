#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/ShaderBuffer.h>
#include<hgl/math/Vector.h>
STD_MTL_NAMESPACE_BEGIN
namespace blinnphong
{
    struct SunLight
    {
        Vector3f direction;
        Vector3f color;
    };//struct SunLight

    constexpr const ShaderBufferSource SBS_SunLight=
    {
        "SunLight",
        "sun",

        R"(
        vec3 direction;
        vec3 color;
)"
    };
}//namespace blinnphong
STD_MTL_NAMESPACE_END
