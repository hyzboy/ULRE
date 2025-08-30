#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

STD_MTL_NAMESPACE_BEGIN
constexpr const ShaderBufferSource SBS_ViewportInfo=
{
    DescriptorSetType::RenderTarget,

    "viewport",
    "ViewportInfo",

    R"(
    mat4 ortho_matrix;

    vec2 canvas_resolution;
    vec2 viewport_resolution;
    vec2 inv_viewport_resolution;
)"
};

constexpr const ShaderBufferSource SBS_CameraInfo=
{
    DescriptorSetType::Camera,

    "camera",
    "CameraInfo",

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

constexpr const ShaderBufferSource SBS_LocalToWorld=
{
    DescriptorSetType::PerFrame,

    "l2w",
    "LocalToWorldData",

    R"(
    mat4 mats[L2W_MAX_COUNT];
)"
};

// UBO必须严格指定数组的大小
// SSBO则不需要，使用[]方式指定为动态大小数组

constexpr const char MaterialInstanceStruct[]="MaterialInstance";

constexpr const ShaderBufferSource SBS_MaterialInstance=
{
    DescriptorSetType::PerMaterial,

    "mtl",
    "MaterialInstanceData",

    R"(
    MaterialInstance mi[MI_MAX_COUNT];)"
};

constexpr const ShaderBufferSource SBS_JointInfo=
{
    DescriptorSetType::PerFrame,

    "joint",
    "JointInfo",

    R"(
        mat4 mats[];
    )"
};

/**
* SkyInfo（全局环境/天空信息）
* - sun_direction: 从太阳指向场景的方向，需归一化，w=0
* - sun_color:     线性空间 RGBA
* - sun_intensity: 光强，夜晚为0
* - sun_path_azimuth_deg: 正午太阳方位角（度），默认 180=南
* - max_elevation_deg:    正午最大仰角（度）
* - enabled:       1=启用，0=禁用
* 预留项以注释保留，后续可启用并接入材质。
*/
constexpr const ShaderBufferSource SBS_SkyInfo=
{
    DescriptorSetType::Global,

    "sky",
    "SkyInfo",

    R"(
    vec4  sun_direction;        // w=0
    vec4  sun_color;            // 线性空间 RGBA

    float sun_intensity;        // 光强（可乘入颜色或单独使用）
    float sun_path_azimuth_deg; // 正午方位角（度）
    float max_elevation_deg;    // 正午最大仰角（度）

    // 大气散射基础色（用于 exp2(-ray.y/atmosphere_base_colors) 计算）
    vec3  atmosphere_base_colors; // 替代硬编码的 vec3(0.1, 0.3, 0.6)
    float sun_angular_size;     // 太阳/月亮的视角大小
    
    // 统一的天体参数（太阳/月亮共用）
    float celestial_halo_intensity; // 光晕强度
    float celestial_halo_power;     // 光晕衰减指数

    // 预留参数（大气散射与曝光等），未来启用：
    // float turbidity;         // 浊度
    // float rayleigh;          // 瑞利散射强度
    // float mie;               // 米氏散射强度
    // float ground_albedo;     // 地表反照率
    // float exposure;          // 曝光
    // vec3  sky_tint;          // 天空色倾向
)"
};
STD_MTL_NAMESPACE_END
