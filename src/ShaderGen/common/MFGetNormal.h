#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/data/CoordinateSystem.h>
namespace hgl::graph::mtl{
namespace func
{
    constexpr const char *GetNormalMatrix=R"(
mat3 GetNormalMatrix()
{
    return mat3(camera.view*GetLocalToWorld());
}
)";

    constexpr const char *GetNormal=R"(
vec3 GetNormal(mat3 normal_matrix,vec3 normal)
{
    return normalize(normal_matrix*normal);
}
)";

    constexpr const char *GetNormalVS=R"(
vec3 GetNormal()
{
    return normalize(mat3(camera.view*GetLocalToWorld())*Normal);
}
)";
}//namespace func
}//namespace hgl::graph::mtl
