#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

STD_MTL_NAMESPACE_BEGIN
namespace func
{
    //@link http://www.opengl-tutorial.org/intermediate-tutorials/billboards-particles/billboards/

constexpr const char *GetCameraViewMatrix=R"(
mat4 GetCameraViewMatrix()
{
    return camera.view*GetLocalToWorld();
}
)";

    constexpr const char *GetCameraViewRight=R"(
vec3 GetViewRight(mat4 cv)
{
    return vec3(cv[0][0],cv[1][0],cv[2][0]);
}
)";

    constexpr const char *GetCameraViewUp=R"(
vec3 GetViewUp(mat4 cv)
{
    return vec3(cv[0][1],cv[1][1],cv[2][1]);
}
)";

    constexpr const char *FrustumCheck=R"(
bool FrustumCheck(vec4 pos, float radius)
{
    for (int i = 0; i < 6; i++) 
    {
        if (dot(pos, camera.frustum_planes[i]) + radius < 0.0)
        {
            return false;
        }
    }
    return true;
}
)";
}//namespace func
STD_MTL_NAMESPACE_END
