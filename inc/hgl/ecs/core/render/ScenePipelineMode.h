#pragma once

#include <cstdint>

namespace hgl::ecs
{
    /**
     * 场景渲染工作流模式 (ScenePipelineMode)
     *
     * 针对中小团队与独立开发者核心场景的硬编码黄金路径，拒绝万能黑盒 FrameGraph。
     * 特定场景类型直接匹配一条最清晰、最健壮、断点可直达的固定管线流程。
     */
    enum class ScenePipelineMode : uint8_t
    {
        Custom = 0,         ///< 自定义/完全由外部手动驱动（不自动插入任何预处理 Pass）
        StandardLitCSM,     ///< 【当前落地黄金路径】标准 3D / FPS / TPS 陆地场景（主光级联阴影全自动托管）

        // ── 未来特定场景预留空壳（未来按需实现硬编码分支） ──
        TopDownRTS,         ///< [预留] RTS 俯视固定倾角大范围阴影
        AerialLowAltitude,  ///< [预留] 近地空战（大倾角地表快速流动）
        AerialHighAltitude, ///< [预留] 高空空战（地表全景超大视距）
        Space3D,            ///< [预留] 太空 3D（无重力多方向星体主光）
        SideScroll2D,       ///< [预留] 横版前向 / 2.5D
    };
}//namespace hgl::ecs
