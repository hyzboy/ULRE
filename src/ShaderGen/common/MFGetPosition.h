#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/CoordinateSystem.h>
STD_MTL_NAMESPACE_BEGIN
namespace func
{
    constexpr const char *GetPosition2D[size_t(CoordinateSystem2D::RANGE_SIZE)]=
    {
        R"(
vec4 GetPosition2D()
{
    return vec4(Position,0,1);
}
)",

R"(
vec4 GetPosition2D()
{
    return vec4(Position.xy*2-1,0,1);
}
)",

R"(
vec4 GetPosition2D()
{
    return viewport.ortho_matrix*vec4(Position,0,1);
}
)"
    };

    constexpr const char *GetPosition2DL2W[size_t(CoordinateSystem2D::RANGE_SIZE)]=
    {
        R"(
vec4 GetPosition2D()
{
    return GetLocalToWorld()*vec4(Position,0,1);
}
)",

R"(
vec4 GetPosition2D()
{
    return GetLocalToWorld()*vec4(Position.xy*2-1,0,1);
}
)",

R"(
vec4 GetPosition2D()
{
    return GetLocalToWorld()*viewport.ortho_matrix*vec4(Position,0,1);
}
)"
    };
}//namespace func
STD_MTL_NAMESPACE_END
