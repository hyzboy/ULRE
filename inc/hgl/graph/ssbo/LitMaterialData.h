#pragma once

#include <hgl/CoreType.h>
#include <hgl/color/Color4f.h>

namespace hgl::graph::ssbo
{
    constexpr float kDefaultLitMaterialNormalStrength = 0.35f;
    constexpr float kDefaultLitMaterialFresnel = 0.04f;

    struct LitMaterialData
    {
        Color4f base_color = Color4f(1.0f); ///<基础颜色
        float   metallic;                  ///<金属度
        float   roughness;                 ///<粗糙度
        float   normal_scale = kDefaultLitMaterialNormalStrength; ///<法线强度(运行时可调)
        float   fresnel      = kDefaultLitMaterialFresnel;        ///<菲尼尔反射率/强度(F0或Fresnel Factor，默认0.04)
    };

    constexpr const size_t LitMaterialDataBytes = sizeof(LitMaterialData);
}
