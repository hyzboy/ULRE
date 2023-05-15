#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/CoordinateSystem.h>
STD_MTL_NAMESPACE_BEGIN
namespace func
{
    constexpr const char *GetPosition2D[size_t(CoordinateSystem2D::RANGE_SIZE)]=
    {
        R"(vec4 GetPosition2D(vec4 pos)
{
    return pos;
})",

R"(vec4 GetPosition2D(vec4 pos)
{
    return vec4(pos.xy*2-1,pos.z,pos.w);
})",

R"(vec4 GetPosition2D(vec4 pos)
{
    return viewport.ortho_matrix*pos;
})"
    };
}//namespace func
STD_MTL_NAMESPACE_END
