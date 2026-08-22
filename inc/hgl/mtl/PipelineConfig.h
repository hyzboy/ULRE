#pragma once

#include <hgl/vk/VK.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    /**
     * CN: 材质管线配置（纯数据，替代旧的 PipelinePreset 枚举）。
     *     所有字段均为显式声明，不再由系统自动猜测（如“VEC2 输入即 2D”）。
     * EN: Material pipeline configuration (pure data, replacing the old PipelinePreset enum).
     *     All fields are explicitly declared; the system no longer guesses from shader inputs.
     */
    struct MaterialPipelineConfig
    {
        VkCullModeFlags cull_mode = VK_CULL_MODE_BACK_BIT;              // 剔除模式
        bool depth_test = true;                                     // 深度测试
        bool depth_write = true;                                    // 深度写入
        VkCompareOp depth_compare_op = VK_COMPARE_OP_GREATER_OR_EQUAL;  // 深度比较（引擎 Vulkan RH_ZO 默认）
        bool alpha_blend = false;                                   // 颜色混合
        bool alpha_to_coverage = false;                             // Alpha-to-coverage 多采样覆盖
        VkBlendFactor blend_src = VK_BLEND_FACTOR_SRC_ALPHA;        // 混合源因子
        VkBlendFactor blend_dst = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // 混合目标因子
        float line_width = 1.0f;                                    // 线宽（启用动态线宽时由命令缓冲覆盖）
        bool dynamic_line_width = false;                            // 启用动态线宽
        bool overlay = false;                                       // 覆盖层渲染路径（替代 IsOverlayPipelinePreset）
        bool wireframe = false;                                     // 线框多边形模式
    };//struct MaterialPipelineConfig

    inline bool operator==(const MaterialPipelineConfig &lhs,
                           const MaterialPipelineConfig &rhs) noexcept
    {
        return lhs.cull_mode == rhs.cull_mode
            && lhs.depth_test == rhs.depth_test
            && lhs.depth_write == rhs.depth_write
            && lhs.depth_compare_op == rhs.depth_compare_op
            && lhs.alpha_blend == rhs.alpha_blend
            && lhs.alpha_to_coverage == rhs.alpha_to_coverage
            && lhs.blend_src == rhs.blend_src
            && lhs.blend_dst == rhs.blend_dst
            && lhs.line_width == rhs.line_width
            && lhs.dynamic_line_width == rhs.dynamic_line_width
            && lhs.overlay == rhs.overlay
            && lhs.wireframe == rhs.wireframe;
    }

    inline bool operator!=(const MaterialPipelineConfig &lhs,
                           const MaterialPipelineConfig &rhs) noexcept
    {
        return !(lhs == rhs);
    }

    /**
     * The single render-state payload consumed by both shader generation and
     * Vulkan pipeline creation.  Material definitions provide the defaults;
     * recipes are resolved into this value before either backend is called.
     */
    struct ResolvedMaterialRenderState
    {
        bool double_sided = false;
        bool alpha_test = false;
        float alpha_cutoff = 0.5f;
        bool dither = false;
        MaterialPipelineConfig pipeline_config;
    };

    struct MaterialRenderStateOverrides
    {
        bool has_double_sided = false;
        bool double_sided = false;
        bool has_alpha_test = false;
        bool alpha_test = false;
        bool has_alpha_cutoff = false;
        float alpha_cutoff = 0.5f;
        bool has_dither = false;
        bool dither = false;
        bool has_pipeline_config = false;
        MaterialPipelineConfig pipeline_config;
    };

    // ── 便捷构造器（每个都返回完整可用的配置，作者可在其上再任意修改字段）──

    inline MaterialPipelineConfig MakeSolid3DConfig()
    {
        return MaterialPipelineConfig{};
    }

    inline MaterialPipelineConfig MakeLineMeshConfig()
    {
        // Line mesh shader（P2：line-to-quad）——quad 三角形绕序不定（clip 空间偏移可能翻转），
        // 必须双面绘制（cull off）；深度行为与 Solid3D 一致（test+write）。
        MaterialPipelineConfig config;
        config.cull_mode = VK_CULL_MODE_NONE;
        return config;
    }

    inline MaterialPipelineConfig MakeAlpha3DConfig()
    {
        MaterialPipelineConfig config;
        config.alpha_blend = true;
        return config;
    }

    inline MaterialPipelineConfig MakeGizmoOverlayConfig()
    {
        MaterialPipelineConfig config;
        config.cull_mode = VK_CULL_MODE_NONE;
        config.depth_write = false;
        config.depth_compare_op = VK_COMPARE_OP_ALWAYS;
        config.overlay = true;
        return config;
    }

    inline MaterialPipelineConfig MakeSolid2DConfig()
    {
        MaterialPipelineConfig config;
        config.cull_mode = VK_CULL_MODE_NONE;
        config.depth_test = false;
        config.depth_write = false;
        config.depth_compare_op = VK_COMPARE_OP_ALWAYS;
        config.overlay = true;
        return config;
    }

    inline MaterialPipelineConfig MakeAlpha2DConfig()
    {
        MaterialPipelineConfig config = MakeSolid2DConfig();
        config.alpha_blend = true;
        return config;
    }

    inline MaterialPipelineConfig MakeSkyConfig()
    {
        MaterialPipelineConfig config;
        config.cull_mode = VK_CULL_MODE_FRONT_BIT;
        config.depth_write = false;
        return config;
    }

    inline uint64_t HashMaterialPipelineConfig(const MaterialPipelineConfig &config) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        h << config.cull_mode
          << config.depth_test
          << config.depth_write
          << config.depth_compare_op
          << config.alpha_blend
          << config.alpha_to_coverage
          << config.blend_src
          << config.blend_dst
          << config.line_width
          << config.dynamic_line_width
          << config.overlay
          << config.wireframe;

        return h;
    }

    inline uint64_t HashResolvedMaterialRenderState(
        const ResolvedMaterialRenderState &state) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        h << state.double_sided
          << state.alpha_test
          << state.alpha_cutoff
          << state.dither
          << HashMaterialPipelineConfig(state.pipeline_config);

        return h;
    }
}//namespace hgl::graph::mtl
