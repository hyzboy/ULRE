#pragma once

#include <hgl/CoreType.h>
#include <hgl/color/Color4f.h>
#include <hgl/graph/BufferAnnotation.h>

namespace hgl::graph::ssbo
{
    constexpr float kDefaultLitMaterialNormalStrength = 0.35f;
    constexpr float kDefaultLitMaterialFresnel = 0.04f;

    /// ⚠ 历史遗留：CPU 侧 Lit 材质参数镜像（GPU 行真源已迁到 `PBRSurfaceRow`，两者字段同形）。
    /// 标注为 CPU-only ⇒ 生成器**不**为它产生 GLSL / 序列化，只做登记。
    struct HGL_CPU_ONLY LitMaterialData
    {
        Color4f base_color = Color4f(1.0f); ///<基础颜色
        float   metallic;                  ///<金属度
        float   roughness;                 ///<粗糙度
        float   normal_scale = kDefaultLitMaterialNormalStrength; ///<法线强度(运行时可调)
        float   fresnel      = kDefaultLitMaterialFresnel;        ///<菲尼尔反射率/强度(F0或Fresnel Factor，默认0.04)
    };

    constexpr const size_t LitMaterialDataBytes = sizeof(LitMaterialData);
}
