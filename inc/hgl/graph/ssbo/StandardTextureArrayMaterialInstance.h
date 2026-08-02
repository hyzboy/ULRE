#pragma once

#include <hgl/CoreType.h>

namespace hgl::graph::ssbo
{
    constexpr float kDefaultTextureArrayMaterialNormalStrength = 0.35f;

    struct StandardTextureArrayMaterialInstance
    {
        uint32 base_color;      ///<基础颜色
        float  metallic;        ///<金属度
        float  roughness;       ///<粗糙度
        float  normal_scale = kDefaultTextureArrayMaterialNormalStrength; ///<法线强度(运行时可调)
    };

    constexpr const size_t StandardTextureArrayMaterialInstanceBytes = sizeof(StandardTextureArrayMaterialInstance);
}
