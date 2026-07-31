#pragma once

#include <hgl/CoreType.h>

namespace hgl::graph::ssbo
{
    constexpr float kDefaultMaterialNormalStrength = 0.35f;

    struct StandardMaterialInstance
    {
        uint32 base_color;      ///<基础颜色
        float  metallic;        ///<金属度
        float  roughness;       ///<粗糙度
        float  normal_scale = kDefaultMaterialNormalStrength; ///<法线强度(运行时可调)
    };

    constexpr const size_t StandardMaterialInstanceBytes = sizeof(StandardMaterialInstance);
}
