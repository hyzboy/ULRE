#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

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

    vec4 frustum_planes[6];

    mat4 sky;

    vec3 pos;                   //eye
    vec3 view_line;             //pos-target
    vec3 world_up;

    vec3 billboard_up;
    vec3 billboard_right;

    float znear,zfar;)"
};

constexpr const char LocalToWorldStruct[]="LocalToWorld";
constexpr const DescriptorSetType DST_LocalToWorld=DescriptorSetType::PerFrame;

constexpr const ShaderBufferSource SBS_LocalToWorld=
{
    "LocalToWorldData",
    "l2w",

    R"(
    mat4 mats[L2W_MAX_COUNT];
)"
};

// UBO必须严格指定数组的大小
// SSBO则不需要，使用[]方式指定为动态大小数组

constexpr const char MaterialInstanceStruct[]="MaterialInstance";
constexpr const DescriptorSetType DST_MaterialInstance=DescriptorSetType::PerMaterial;

constexpr const ShaderBufferSource SBS_MaterialInstance=
{
    "MaterialInstanceData",
    "mtl",

    R"(
    MaterialInstance mi[MI_MAX_COUNT];)"
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
