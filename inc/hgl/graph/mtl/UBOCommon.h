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
    DescriptorSetType::Scene,

    "sky",
    "SkyInfo",

    R"(
    vec4  sun_direction;        // w=0
    vec4  sun_color;            // 线性空间 RGBA

    vec4  base_sky_color;       // 天空基础色
    vec4  moon_color;
    vec4  halo_color;

    float sun_intensity;        // 光强（可乘入颜色或单独使用）
    float sun_path_azimuth_deg; // 正午方位角（度）
    float max_elevation_deg;    // 正午最大仰角（度）
    float latitude_deg;         // 纬度（度）

    float longitude_deg;        // 经度（度）
    float altitude_m;           // 海拔（米）
    float moon_intensity;       // 月亮强度
    float halo_intensity;       // 光晕强度

    ivec4 date;                 // year, month, day, padding

    // 预留参数（大气散射与曝光等），未来启用：
    // float turbidity;         // 浊度
    // float rayleigh;          // 瑞利散射强度
    // float mie;               // 米氏散射强度
    // float ground_albedo;     // 地表反照率
    // float exposure;          // 曝光
    // float sun_angular_deg;   // 太阳视半径（度）
    // vec3  sky_tint;          // 天空色倾向
)"
};
STD_MTL_NAMESPACE_END
