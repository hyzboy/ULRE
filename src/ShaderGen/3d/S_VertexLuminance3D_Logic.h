#pragma once

#include <hgl/graph/mtl/ShaderLogic.h>

namespace hgl::graph::mtl {

constexpr const char* VERTEX_LUMINANCE_3D_VERTEX_RESOURCES[] = {
    "camera",
    "l2w",
    "mtl"
};

constexpr const char* VERTEX_LUMINANCE_3D_VERTEX_HELPERS[] = {
    "GetMI"
};

constexpr const char* const* VERTEX_LUMINANCE_3D_FRAGMENT_RESOURCES = nullptr;
constexpr const char* const* VERTEX_LUMINANCE_3D_FRAGMENT_HELPERS = nullptr;

const VertexShaderLogic VERTEX_LUMINANCE_3D_VERTEX_SHADER_LOGIC = {
    {
        VERTEX_LUMINANCE_3D_VS_BUSINESS_VEC3,
        nullptr,
        VERTEX_LUMINANCE_3D_VERTEX_RESOURCES,
        3,
        VERTEX_LUMINANCE_3D_VERTEX_HELPERS,
        1
    }
};

const FragmentShaderLogic VERTEX_LUMINANCE_3D_FRAGMENT_SHADER_LOGIC = {
    {
        VERTEX_LUMINANCE_3D_FS_BUSINESS,
        nullptr,
        VERTEX_LUMINANCE_3D_FRAGMENT_RESOURCES,
        0,
        VERTEX_LUMINANCE_3D_FRAGMENT_HELPERS,
        0
    }
};

const MaterialLogicDef VERTEX_LUMINANCE_3D_LOGIC = {
    VERTEX_LUMINANCE_3D_VERTEX_SHADER_LOGIC,
    VERTEX_LUMINANCE_3D_FRAGMENT_SHADER_LOGIC,
    nullptr,
    nullptr,
    nullptr
};

const VertexShaderLogic VERTEX_LUMINANCE_3D_VERTEX_SHADER_LOGIC_VEC2 = {
    {
        VERTEX_LUMINANCE_3D_VS_BUSINESS_VEC2,
        nullptr,
        VERTEX_LUMINANCE_3D_VERTEX_RESOURCES,
        3,
        VERTEX_LUMINANCE_3D_VERTEX_HELPERS,
        1
    }
};

const MaterialLogicDef VERTEX_LUMINANCE_3D_LOGIC_VEC2 = {
    VERTEX_LUMINANCE_3D_VERTEX_SHADER_LOGIC_VEC2,
    VERTEX_LUMINANCE_3D_FRAGMENT_SHADER_LOGIC,
    nullptr,
    nullptr,
    nullptr
};

} // namespace hgl::graph::mtl
