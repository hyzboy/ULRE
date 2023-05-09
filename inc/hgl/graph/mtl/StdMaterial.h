﻿#pragma once

#define STD_MTL_NAMESPACE_BEGIN namespace hgl{namespace graph{namespace mtl{
#define STD_MTL_NAMESPACE_END   }}}

#define STD_MTL_NAMESPACE_USING using namespace hgl::graph::mtl;

#include<hgl/TypeFunc.h>
#include<hgl/graph/mtl/ShaderBuffer.h>

STD_MTL_NAMESPACE_BEGIN

enum class CoordinateSystem2D
{
    NDC,
    ZeroToOne,          //左上角为0,0；右下角为1,1
    Ortho,              //左上角为0,0；右下角为(width-1),(height-1)

    ENUM_CLASS_RANGE(NDC,Ortho)
};

constexpr const ShaderBufferSource SBS_ViewportInfo=
{
    "ViewportInfo",
    "viewport",

    R"(
mat4 ortho_matrix;

vec2 canvas_resolution;
vec2 viewport_resolution;
vec2 inv_viewport_resolution;
)"
};

constexpr const ShaderBufferSource SBS_CameraInfo=
{
    "CameraInfo",
    "camera",

    R"(
mat4 projection;
mat4 inverse_projection;

mat4 view;
mat4 inverse_view;

mat4 vp;
mat4 inverse_vp;

mat4 sky;

vec3 pos;                   //eye
vec3 view_line;             //pos-target
vec3 world_up;

float znear,zfar;)"
};

constexpr const ShaderBufferSource SBS_BoneInfo=
{
    "BoneInfo",
    "bone",

R"(
    mat4 bone_mats[];
)"
};

namespace func
{
    constexpr const char GetLocalToWorld[]=R"(
mat4 GetLocalToWorld()
{
    return mat4(LocalToWorld_0,
                LocalToWorld_1,
                LocalToWorld_2,
                LocalToWorld_3);
}
)";

    constexpr const char GetBoneMatrix[]=R"(
mat4 GetBoneMatrix()
{
    return  bone_mats[BoneID.x]*BoneWeight.x+
            bone_mats[BoneID.y]*BoneWeight.y+
            bone_mats[BoneID.z]*BoneWeight.z+
            bone_mats[BoneID.w]*BoneWeight.w;
}
)";
}//namespace func
STD_MTL_NAMESPACE_END
