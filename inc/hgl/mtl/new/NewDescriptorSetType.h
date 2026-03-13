#pragma once

#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    // 新 4-Set 布局：与旧 DescriptorSetType 并存，迁移完成后替换
    enum class NewDescriptorSetType : uint8
    {
        PerScene    = 0,    // Set 0: 全局/每帧数据 (ViewportInfo, CameraInfo, SkyInfo, LightBuffer)
        PerView     = 1,    // Set 1: 每视图数据 (L2W SSBO)
        PerMaterial = 2,    // Set 2: 每材质数据 (MI SSBO, 纹理槽)
        PerDraw     = 3,    // Set 3: 环境/管线 RT (ShadowMap, SSAO, IBL, HZB, Cluster, Fog, ...)

        COUNT = 4
    };
}
