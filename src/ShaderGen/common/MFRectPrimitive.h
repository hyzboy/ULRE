#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

STD_MTL_NAMESPACE_BEGIN
namespace func
{
    constexpr const char *GetPosition2DRect[size_t(CoordinateSystem2D::RANGE_SIZE)]=
    {
        R"(
vec4 GetPosition2D()
{
    return Position;
}
)",

R"(
vec4 GetPosition2D()
{
    return Position*2-1;
}
)",

R"(
vec4 GetPosition2D()
{
    vec4 lt=viewport.ortho_matrix*vec4(Position.xy,0,1);
    vec4 rb=viewport.ortho_matrix*vec4(Position.zw,0,1);

    return vec4(lt.xy,rb.xy);
}
)"
    };

    constexpr const char *GetPosition2DRectL2W[size_t(CoordinateSystem2D::RANGE_SIZE)]=
    {
        R"(
vec4 GetPosition2D()
{
    vec4 lt=GetLocalToWorld()*vec4(Position.xy,0,1);
    vec4 rb=GetLocalToWorld()*vec4(Position.zw,0,1);

    return vec4(lt.xy,rb.xy);
}
)",

R"(
vec4 GetPosition2D()
{
    vec4 lt=GetLocalToWorld()*vec4(Position.xy,0,1);
    vec4 rb=GetLocalToWorld()*vec4(Position.zw,0,1);

    return vec4(lt.xy,rb.xy)*2-1;
}
)",

R"(
vec4 GetPosition2D()
{
    vec4 lt=viewport.ortho_matrix*GetLocalToWorld()*vec4(Position.xy,0,1);
    vec4 rb=viewport.ortho_matrix*GetLocalToWorld()*vec4(Position.zw,0,1);

    return vec4(lt.xy,rb.xy);
}
)"
    };
}//namespace func
STD_MTL_NAMESPACE_END
