#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/ShaderBuffer.h>

STD_MTL_NAMESPACE_BEGIN
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

constexpr const char MaterialInstanceStruct[]="MaterialInstance";

constexpr const ShaderBufferSource SBS_MaterialInstanceData=
{
    "MaterialInstanceData",
    "mtl",

    R"(
    MaterialInstance mi[256];)"
};

constexpr const ShaderBufferSource SBS_JointInfo=
{
    "JointInfo",
    "joint",

R"(
    mat4 mats[];
)"
};
STD_MTL_NAMESPACE_END
