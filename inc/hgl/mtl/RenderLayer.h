#pragma once

#include <cstdint>

namespace hgl::graph::mtl
{
    /**
     * CN: 渲染分段（固定渲染层级，替代「按内容自适应派生的 pass」）。
     *     每个绘制批次在收集阶段被归入唯一一个层级，渲染时按枚举值从小到大
     *     依次提交。层级之间**互不重叠**，由固定的 pass 分段表驱动，不再从
     *     场景内容反推渲染顺序。
     * EN: Fixed render segmentation. Every draw batch is assigned exactly one
     *     layer at collect time; layers are submitted in ascending enum order
     *     by a fixed pass table, never derived from scene content.
     */
    enum class RenderLayer : uint8_t
    {
        Scene = 0,     // 深度测试的 3D 场景（含天空球等场景几何）
        Gizmo = 1,     // 无深度测试、必须最后绘制于场景之上的覆盖层（Gizmo）
        UI2D = 2,      // 2D UI（sprite 桥接的 2D 图元 / 常规 HUD 文本）
        Overlay = 3,   // 最终全屏覆盖层（DEBUG PROF 等统计文本）
    };

    /** 有效层级数量（不含非法哨兵）*/
    constexpr uint8_t kRenderLayerCount = 4;
}//namespace hgl::graph::mtl
