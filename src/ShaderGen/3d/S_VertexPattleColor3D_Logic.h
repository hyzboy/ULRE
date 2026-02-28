#pragma once

#include <hgl/graph/mtl/ShaderLogic.h>

namespace hgl::graph::mtl {

constexpr const char* VERTEX_PATTLE_COLOR_3D_VERTEX_RESOURCES[] = {
    "l2w",
    "camera",
    "color_pattle"
};

constexpr const char* const* VERTEX_PATTLE_COLOR_3D_VERTEX_HELPERS = nullptr;

constexpr const char* const* VERTEX_PATTLE_COLOR_3D_FRAGMENT_RESOURCES = nullptr;
constexpr const char* const* VERTEX_PATTLE_COLOR_3D_FRAGMENT_HELPERS = nullptr;

const VertexShaderLogic VERTEX_PATTLE_COLOR_3D_VERTEX_SHADER_LOGIC = {
    {
        VERTEX_PATTLE_COLOR_3D_VS_BUSINESS,
        nullptr,
        VERTEX_PATTLE_COLOR_3D_VERTEX_RESOURCES,
        3,
        VERTEX_PATTLE_COLOR_3D_VERTEX_HELPERS,
        0
    }
};

const FragmentShaderLogic VERTEX_PATTLE_COLOR_3D_FRAGMENT_SHADER_LOGIC = {
    {
        VERTEX_PATTLE_COLOR_3D_FS_BUSINESS,
        nullptr,
        VERTEX_PATTLE_COLOR_3D_FRAGMENT_RESOURCES,
        0,
        VERTEX_PATTLE_COLOR_3D_FRAGMENT_HELPERS,
        0
    }
};

const MaterialLogicDef VERTEX_PATTLE_COLOR_3D_LOGIC = {
    VERTEX_PATTLE_COLOR_3D_VERTEX_SHADER_LOGIC,
    VERTEX_PATTLE_COLOR_3D_FRAGMENT_SHADER_LOGIC,
    nullptr,
    nullptr,
    nullptr
};

} // namespace hgl::graph::mtl
