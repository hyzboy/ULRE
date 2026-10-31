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
     * 严格弱序（供有序容器 / 合批键的 `operator<` 使用）。
     * 与 `operator==` 逐字段对齐：相等 ⇒ 两个方向都返回 false。
     * 哈希（`HashMaterialPipelineConfig`）不能替代它——哈希相等不代表配置相同。
     */
    inline bool LessMaterialPipelineConfig(const MaterialPipelineConfig &lhs,
                                           const MaterialPipelineConfig &rhs) noexcept
    {
        if (lhs.cull_mode          != rhs.cull_mode         ) return lhs.cull_mode          < rhs.cull_mode;
        if (lhs.depth_test         != rhs.depth_test        ) return lhs.depth_test         < rhs.depth_test;
        if (lhs.depth_write        != rhs.depth_write       ) return lhs.depth_write        < rhs.depth_write;
        if (lhs.depth_compare_op   != rhs.depth_compare_op  ) return lhs.depth_compare_op   < rhs.depth_compare_op;
        if (lhs.alpha_blend        != rhs.alpha_blend       ) return lhs.alpha_blend        < rhs.alpha_blend;
        if (lhs.blend_src          != rhs.blend_src         ) return lhs.blend_src          < rhs.blend_src;
        if (lhs.blend_dst          != rhs.blend_dst         ) return lhs.blend_dst          < rhs.blend_dst;
        if (lhs.line_width         != rhs.line_width        ) return lhs.line_width         < rhs.line_width;
        if (lhs.dynamic_line_width != rhs.dynamic_line_width) return lhs.dynamic_line_width < rhs.dynamic_line_width;
        if (lhs.overlay            != rhs.overlay           ) return lhs.overlay            < rhs.overlay;
        return lhs.wireframe < rhs.wireframe;
    }

    /**
     * `MaterialPipelineConfig` 的**字段存在性**位掩码。
     *
     * 配方里的 `[render_state.pipeline]` 是**局部覆盖**：作者只写与默认不同的键，
     * 未写的字段保留契约/结构体默认值。掩码记录「哪些字段被显式声明过」，
     * 是唯一能让 `false` / `0` / `None` 这类合法取值与「没写」区分开的机制
     * （不能拿字段值本身当哨兵）。
     */
    enum class MaterialPipelineField : uint16_t
    {
        CullMode         = 1u << 0,
        DepthTest        = 1u << 1,
        DepthWrite       = 1u << 2,
        DepthCompareOp   = 1u << 3,
        AlphaBlend       = 1u << 4,
        BlendSrc         = 1u << 5,
        BlendDst         = 1u << 6,
        LineWidth        = 1u << 7,
        DynamicLineWidth = 1u << 8,
        Overlay          = 1u << 9,
        Wireframe        = 1u << 10,
    };

    using MaterialPipelineFieldMask = uint16_t;

    /** 全部 11 个字段都显式声明（= 整份配置替换）*/
    constexpr MaterialPipelineFieldMask kMaterialPipelineAllFields = 0x7FFu;

    inline bool MaterialPipelineHas(const MaterialPipelineFieldMask mask,
                                    const MaterialPipelineField field) noexcept
    {
        return (mask & static_cast<MaterialPipelineFieldMask>(field)) != 0;
    }

    /** 按掩码把 `over` 中**已声明**的字段盖到 `base` 上（`mask == 0` ⇒ 不动）*/
    inline void MergeMaterialPipelineConfig(MaterialPipelineConfig &base,
                                            const MaterialPipelineConfig &over,
                                            const MaterialPipelineFieldMask mask) noexcept
    {
        if (MaterialPipelineHas(mask, MaterialPipelineField::CullMode        )) base.cull_mode          = over.cull_mode;
        if (MaterialPipelineHas(mask, MaterialPipelineField::DepthTest       )) base.depth_test         = over.depth_test;
        if (MaterialPipelineHas(mask, MaterialPipelineField::DepthWrite      )) base.depth_write        = over.depth_write;
        if (MaterialPipelineHas(mask, MaterialPipelineField::DepthCompareOp  )) base.depth_compare_op   = over.depth_compare_op;
        if (MaterialPipelineHas(mask, MaterialPipelineField::AlphaBlend      )) base.alpha_blend        = over.alpha_blend;
        if (MaterialPipelineHas(mask, MaterialPipelineField::BlendSrc        )) base.blend_src          = over.blend_src;
        if (MaterialPipelineHas(mask, MaterialPipelineField::BlendDst        )) base.blend_dst          = over.blend_dst;
        if (MaterialPipelineHas(mask, MaterialPipelineField::LineWidth       )) base.line_width         = over.line_width;
        if (MaterialPipelineHas(mask, MaterialPipelineField::DynamicLineWidth)) base.dynamic_line_width = over.dynamic_line_width;
        if (MaterialPipelineHas(mask, MaterialPipelineField::Overlay         )) base.overlay            = over.overlay;
        if (MaterialPipelineHas(mask, MaterialPipelineField::Wireframe       )) base.wireframe          = over.wireframe;
    }

    /**
     * The single render-state payload consumed by both shader generation and
     * Vulkan pipeline creation.  Material definitions provide the defaults;
     * recipes are resolved into this value before either backend is called.
     */
    struct ResolvedMaterialRenderState
    {
        bool alpha_test = false;
        float alpha_cutoff = 0.5f;
        bool dither = false;
        MaterialPipelineConfig pipeline_config;
    };

    struct MaterialRenderStateOverrides
    {
        bool has_alpha_test = false;
        bool alpha_test = false;
        bool has_alpha_cutoff = false;
        float alpha_cutoff = 0.5f;
        bool has_dither = false;
        bool dither = false;
        bool has_pipeline_config = false;
        MaterialPipelineConfig pipeline_config;
        /**
         * `pipeline_config` 中**哪些字段被显式声明**。
         *
         * `0` = 没有逐字段信息：`pipeline_config` 当作**整份配置**使用
         *      （C++ 侧直接赋值 / `NormalizeRecipe` 回写的既有用法）。
         * `kMaterialPipelineAllFields` = 同样整份替换，但语义上是「全写了」。
         * 其余值 = 局部覆盖，只盖已声明的位。
         *
         * ⚠ 掩码**不进** `HashMaterialRecipe`：归一化后掩码恒为 `0`、字段值已是
         * 合成后的权威值，值集合本身就足以构成身份；把掩码折进哈希只会让
         * 「同一份状态、不同书写方式」分裂成不同键。
         */
        MaterialPipelineFieldMask pipeline_config_mask = 0;
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
        // Sky geometry is viewed from inside; winding is not a visibility filter.
        config.cull_mode = VK_CULL_MODE_NONE;
        config.depth_write = false;
        return config;
    }

    inline MaterialPipelineConfig MakeSkyCubeSphereConfig()
    {
        MaterialPipelineConfig config = MakeSkyConfig();
        // This sample uses a dedicated face-culling mode instead of the inside-view sky default.
        config.cull_mode = VK_CULL_MODE_FRONT_BIT;
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

        h << state.alpha_test
          << state.alpha_cutoff
          << state.dither
          << HashMaterialPipelineConfig(state.pipeline_config);

        return h;
    }
}//namespace hgl::graph::mtl
