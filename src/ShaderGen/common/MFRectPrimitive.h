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
    return vec4(Position.xy*2-1,Position.zw);
}
)",

R"(
vec4 GetPosition2D()
{
    vec4 tmp=viewport.ortho_matrix*vec4(Position.xy,0,1);

    return vec4(tmp.xy,Position.zw);
}
)"
    };

    constexpr const char RectVertexPosition[]=R"(
vec4 RectVertexPosition(vec4 pos)
{
    vec4 lt=vec4(pos.xy,vec2(0,1));
    vec4 rb=vec4(pos.zw,vec2(0,1));
    vec4 lt_fin=g_camera.ortho*lt;
    vec4 rb_fin=g_camera.ortho*rb;

    return vec4(lt_fin.xy,rb_fin.xy);
}
)";
}//namespace func
STD_MTL_NAMESPACE_END
