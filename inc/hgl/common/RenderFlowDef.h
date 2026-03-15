#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/mtl/new/QualityTier.h>
#include <cstdint>
#include <cstdio>
#include <string>

namespace hgl::graph::mtl
{
    enum class PipelineInputMode : uint8
    {
        LegacyVABVBO = 0,
        VertexInput = LegacyVABVBO,
        SSBOVertexInput,
        HybridVABVBOAndSSBO,
        AutoPreferSSBO,
        AutoPreferLegacy,
        AutoByCapability,

        ENUM_CLASS_RANGE(LegacyVABVBO, AutoByCapability)
    };

    enum class VertexInputMigrationStage : uint8
    {
        LegacyOnly = 0,
        DualPathValidation,
        PreferSSBO,
        SSBOOnly,

        ENUM_CLASS_RANGE(LegacyOnly, SSBOOnly)
    };

    struct VertexInputMigrationPolicy
    {
        VertexInputMigrationStage stage = VertexInputMigrationStage::PreferSSBO;
        bool allow_legacy_fallback = true;
        bool keep_dual_path_shader_variants = true;
        bool require_consistent_mode_in_flow = true;
        PipelineInputMode default_input_mode = PipelineInputMode::AutoPreferSSBO;
    };

    // 渲染通道标记（通用 RT 通道掩码）
    enum class RenderChannel : uint32
    {
        None = 0,
        Color = 1u << 0,
        Normal = 1u << 1,
        Depth = 1u << 2,
        Emissive = 1u << 3,
        MotionVector = 1u << 4,
        Specular = 1u << 5,
        Roughness = 1u << 6,
        Metallic = 1u << 7,
        AO = 1u << 8,
    };

    inline RenderChannel operator|(RenderChannel a, RenderChannel b)
    {
        return RenderChannel(uint32_t(a) | uint32_t(b));
    }

    inline RenderChannel operator&(RenderChannel a, RenderChannel b)
    {
        return RenderChannel(uint32_t(a) & uint32_t(b));
    }

    inline RenderChannel &operator|=(RenderChannel &a, RenderChannel b)
    {
        a = a | b;
        return a;
    }

    enum class RenderStage : uint8
    {
        EarlyZ_Solid = 0,
        EarlyZ_Masked,
        ShadowMap_Dynamic,
        ShadowMap_Cached,
        ShadowMap_Cascade,
        VisibilityBuffer_Fill,
        Forward_Opaque,
        Forward_Masked,
        Forward_Transparent,
        Forward_Sky,
        HZB_Generation,
        HZB_Culling,
        PostProcess_TAA,
        PostProcess_Bloom,
        PostProcess_ToneMap,
        PostProcess_FXAA,
        PostProcess_MotionBlur,
        PostProcess_DOF,
        PostProcess_SSR,
        PostProcess_SSAO,
        Debug_Visualization,

        ENUM_CLASS_RANGE(EarlyZ_Solid, Debug_Visualization)
    };

    // 使用 hgl::graph::QualityTier（定义在 <hgl/mtl/new/QualityTier.h>）
    using hgl::graph::QualityTier;

    struct RenderPassDefinition
    {
        RenderStage stage;
        bool depth_test;
        bool depth_write;
        RenderChannel read_channels;
        RenderChannel write_channels;
        PipelineCoverageMode coverage_mode;
        bool mandatory;
        PipelineInputMode input_mode = PipelineInputMode::AutoByCapability;
    };

    enum class RenderFlowPreset : uint8
    {
        Forward_Basic = 0,
        Forward_WithEarlyZ,
        ForwardPlus_SingleHZB,
        ForwardPlus_DoubleHZB,
        VisibilityBuffer_Deferred,
        Mobile_Forward,

        ENUM_CLASS_RANGE(Forward_Basic, Mobile_Forward)
    };

    struct RenderFlowDefinition
    {
        const char *name;
        RenderFlowPreset preset;
        const RenderPassDefinition *passes;
        uint32_t pass_count;
        bool mobile_optimized;
        bool requires_compute_shader;
        QualityTier min_required_quality;
        QualityTier max_supported_quality;

        struct ResourceRequirement
        {
            uint32_t color_attachment_count;
            uint32_t depth_attachment_count;
            bool need_hzb_texture;
            bool need_shadow_map;
        } resource_requirements;

        VertexInputMigrationPolicy vertex_input_policy;
    };


}//namespace hgl::graph::mtl
