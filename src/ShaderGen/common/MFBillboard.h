#pragma once

#include"MFCamera.h"
#include<hgl/graph/mtl/ShaderBufferSource.h>

STD_MTL_NAMESPACE_BEGIN
namespace func
{
    //@link http://www.opengl-tutorial.org/intermediate-tutorials/billboards-particles/billboards/

    constexpr const ShaderBufferSource SBS_Billboard=
    {
        "BillboardData",
        "billboard",

        R"(
vec2 center;
vec2 size;
        )"
    };

    constexpr const char *GetBillboardPosition=R"(
vec4 GetBillboardPosition()
{
    mat4 cv_mat     =GetCameraViewMatrix();

    vec3 cv_right   =GetViewRight(cv_mat);
    vec3 cv_up      =GetViewUp(cv_mat);

    vec3 pos        =GetPosition3D();

    vec3(billboard.center,0)+
    cv_right*pos.x*billboard.size.x+
    cv_up   *pos.y*billboard.size.y;
}
)";
}//namespace func
STD_MTL_NAMESPACE_END
