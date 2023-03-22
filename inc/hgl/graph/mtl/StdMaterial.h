#pragma once

#define STD_MTL_NAMESPACE_BEGIN namespace hgl{namespace graph{namespace mtl{
#define STD_MTL_NAMESPACE_END   }}}

#define STD_MTL_NAMESPACE_USING using namespace hgl::graph::mtl

enum class CoordinateSystem2D
{
    NDC,
    ZeroToOne,          //左上角为0,0；右下角为1,1
    Ortho               //左上角为0,0；右下角为(width-1),(height-1)
};

namespace GlobalDescriptor
{
    struct ShaderStruct
    {
        const char *struct_name;
        const char *name;
        const char *codes;
    };

    constexpr const ShaderStruct ViewportInfo=
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

    constexpr const ShaderStruct CameraInfo=
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
}
