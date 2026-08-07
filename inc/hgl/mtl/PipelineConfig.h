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

    // ── 便捷构造器（每个都返回完整可用的配置，作者可在其上再任意修改字段）──

    inline MaterialPipelineConfig MakeSolid3DConfig()
    {
        return MaterialPipelineConfig{};
    }

    inline MaterialPipelineConfig MakeAlpha3DConfig()
    {
        MaterialPipelineConfig config;
        config.alpha_blend = true;
        return config;
    }

    inline MaterialPipelineConfig MakeDynamicLineWidth3DConfig()
    {
        MaterialPipelineConfig config;
        config.dynamic_line_width = true;
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
        uint64 hash = hgl::hash::FNV1aInit<uint64>();

        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.cull_mode);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.depth_test);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.depth_write);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.depth_compare_op);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.alpha_blend);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.alpha_to_coverage);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.blend_src);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.blend_dst);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.line_width);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.dynamic_line_width);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.overlay);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, config.wireframe);

        return static_cast<uint64_t>(hash);
    }
}//namespace hgl::graph::mtl
