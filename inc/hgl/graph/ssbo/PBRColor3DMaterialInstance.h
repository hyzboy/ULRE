#pragma once

#include <hgl/CoreType.h>

namespace hgl::graph::ssbo
{
    struct PBRColor3DMaterialInstance
    {
        uint32 base_color;      ///<基础颜色 (RGBA packed, unpackUnorm4x8 in shader)
        float  metallic;        ///<金属度 [0, 1]
        float  roughness;       ///<粗糙度 [0.04, 1]
    };

    constexpr const size_t PBRColor3DMaterialInstanceBytes = sizeof(PBRColor3DMaterialInstance);
}
