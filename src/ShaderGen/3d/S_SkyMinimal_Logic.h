#pragma once

#include <hgl/graph/mtl/ShaderLogic.h>

namespace hgl::graph::mtl {

constexpr const char* SKY_MINIMAL_VERTEX_RESOURCES[] = {
    "camera",
    "l2w"
};

constexpr const char* const* SKY_MINIMAL_VERTEX_HELPERS = nullptr;

constexpr const char* SKY_MINIMAL_FRAGMENT_RESOURCES[] = {
    "sky"
};

constexpr const char* const* SKY_MINIMAL_FRAGMENT_HELPERS = nullptr;

const VertexShaderLogic SKY_MINIMAL_VERTEX_SHADER_LOGIC = {
    {
        SKY_MINIMAL_VS_BUSINESS,
        nullptr,
        SKY_MINIMAL_VERTEX_RESOURCES,
        2,
        SKY_MINIMAL_VERTEX_HELPERS,
        0
    }
};

const FragmentShaderLogic SKY_MINIMAL_FRAGMENT_SHADER_LOGIC = {
    {
        SKY_MINIMAL_FS_BUSINESS,
        nullptr,
        SKY_MINIMAL_FRAGMENT_RESOURCES,
        1,
        SKY_MINIMAL_FRAGMENT_HELPERS,
        0
    }
};

const MaterialLogicDef SKY_MINIMAL_LOGIC = {
    SKY_MINIMAL_VERTEX_SHADER_LOGIC,
    SKY_MINIMAL_FRAGMENT_SHADER_LOGIC,
    nullptr,
    nullptr,
    nullptr
};

} // namespace hgl::graph::mtl
