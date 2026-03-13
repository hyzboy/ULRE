#pragma once

#include <hgl/type/EnumUtil.h>
#include <cstdint>
#include <cstdio>
#include <string>

namespace hgl::graph::mtl
{
    enum class PipelineRenderPath : uint8
    {
        Forward = 0,
        VBufferDeferred,
        PostProcess,

        ENUM_CLASS_RANGE(Forward, PostProcess)
    };

    enum class PipelineCoverageMode : uint8
    {
        Solid = 0,
        Mask,
        DepthOnlySolid,
        DepthOnlyMask,

        ENUM_CLASS_RANGE(Solid, DepthOnlyMask)
    };

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

    enum class PipelineTopology : uint8
    {
        VSFS = 0,
        MeshFS,
        AutoByCapability,

        ENUM_CLASS_RANGE(VSFS, AutoByCapability)
    };

    // 渲染通道标记（原 GBufferChannel，实际用途为通用 RT 通道掩码）
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

    // 兼容别名（后续全部替换后删除）
    using GBufferChannel = RenderChannel;

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

    enum class PipelineForwardLightingMode : uint8
    {
        PerPixel = 0,
        PerVertex,
        AutoByCapability,

        ENUM_CLASS_RANGE(PerPixel, AutoByCapability)
    };

    enum class NormalEncodingMode : uint8
    {
        None = 0,
        Octahedral,
        Spheremap,

        ENUM_CLASS_RANGE(None, Spheremap)
    };

    struct NormalCompressionPolicy
    {
        bool compress_vertex_input_normal = false;
        NormalEncodingMode vertex_input_encoding = NormalEncodingMode::Octahedral;

        bool compress_normal_map = false;
        NormalEncodingMode normal_map_encoding = NormalEncodingMode::Octahedral;
    };

    struct PipelineMode
    {
        PipelineRenderPath render_path = PipelineRenderPath::Forward;
        PipelineCoverageMode coverage = PipelineCoverageMode::Solid;
        PipelineInputMode input_mode = PipelineInputMode::AutoByCapability;
        PipelineTopology topology = PipelineTopology::AutoByCapability;
        RenderChannel postprocess_output_channels = RenderChannel::None;
        PipelineForwardLightingMode forward_lighting = PipelineForwardLightingMode::PerPixel;
        NormalCompressionPolicy normal_compression;
    };

    enum class RenderStage : uint8
    {
        EarlyZ_Solid = 0,
        EarlyZ_Masked,
        ShadowMap_Directional,
        ShadowMap_Spot,
        ShadowMap_Point,
        VisibilityBuffer_Fill,
        Forward_Opaque,
        Forward_Masked,
        Forward_Transparent,
        Forward_Additive,
        HZB_Generation,
        HZB_Culling,
        PostProcess_TAA,
        PostProcess_Bloom,
        PostProcess_ToneMapping,
        PostProcess_FXAA,
        PostProcess_MotionBlur,
        PostProcess_DOF,
        PostProcess_SSR,
        PostProcess_SSAO,
        Debug_Visualization,

        ENUM_CLASS_RANGE(EarlyZ_Solid, Debug_Visualization)
    };

    // QualityTier placeholder — will be replaced in Stage 2
    enum class QualityTier : uint8
    {
        Low = 0,
        LowPlus,
        Medium,
        MediumPlus,
        High,
        HighPlus,
        Ultra,

        ENUM_CLASS_RANGE(Low, Ultra)
    };

    // 兼容别名（后续全部替换后删除）
    using GBufferQualityPreset = QualityTier;

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
