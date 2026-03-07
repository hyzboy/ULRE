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
        GBufferDeferred,
        VBufferDeferred,
        MobileSubpassGBufferDeferred,
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

    enum class GBufferChannel : uint32
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

    inline GBufferChannel operator|(GBufferChannel a, GBufferChannel b)
    {
        return GBufferChannel(uint32_t(a) | uint32_t(b));
    }

    inline GBufferChannel operator&(GBufferChannel a, GBufferChannel b)
    {
        return GBufferChannel(uint32_t(a) & uint32_t(b));
    }

    inline GBufferChannel &operator|=(GBufferChannel &a, GBufferChannel b)
    {
        a = a | b;
        return a;
    }

    enum class GBufferFormatLevel : uint8
    {
        MobileLite = 0,
        MobileExtended,
        DesktopStandard,
        DesktopFull,
        Custom,

        ENUM_CLASS_RANGE(MobileLite, Custom)
    };

    struct GBufferFormatSpec
    {
        GBufferFormatLevel level = GBufferFormatLevel::MobileLite;
        GBufferChannel channel_mask = GBufferChannel::None;
        const char *generated_param_struct_name = "GBufferParams";
        bool auto_generate_param_struct = true;
        bool enable_motion_vector = false;
    };

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

        bool compress_gbuffer_normal = false;
        NormalEncodingMode gbuffer_encoding = NormalEncodingMode::Octahedral;
    };

    struct PipelineMode
    {
        PipelineRenderPath render_path = PipelineRenderPath::Forward;
        PipelineCoverageMode coverage = PipelineCoverageMode::Solid;
        PipelineInputMode input_mode = PipelineInputMode::AutoByCapability;
        PipelineTopology topology = PipelineTopology::AutoByCapability;
        GBufferFormatSpec gbuffer_format;
        GBufferChannel postprocess_output_channels = GBufferChannel::None;
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
        GBuffer_Opaque,
        GBuffer_Masked,
        VisibilityBuffer_Fill,
        Deferred_Lighting,
        Deferred_LightingTiled,
        Deferred_LightingClustered,
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

    enum class GBufferQualityPreset : uint8
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

    struct GBufferConfiguration
    {
        GBufferQualityPreset preset;
        GBufferFormatLevel level;
        GBufferChannel channel_mask;
        bool enable_motion_vector;
        NormalCompressionPolicy normal_compression;
        uint32_t variant_hash;

        uint32_t RecomputeVariantHash() const;
    };

    constexpr uint32_t HashFNV1a32(uint32_t seed, uint32_t value)
    {
        return (seed ^ value) * 16777619u;
    }

    constexpr uint32_t ComputeGBufferVariantHash(
        GBufferQualityPreset preset,
        GBufferFormatLevel level,
        GBufferChannel channel_mask,
        bool enable_motion_vector,
        const NormalCompressionPolicy &normal_compression)
    {
        uint32_t h = 2166136261u;

        h = HashFNV1a32(h, uint32_t(preset));
        h = HashFNV1a32(h, uint32_t(level));
        h = HashFNV1a32(h, uint32_t(channel_mask));
        h = HashFNV1a32(h, enable_motion_vector ? 1u : 0u);

        h = HashFNV1a32(h, normal_compression.compress_vertex_input_normal ? 1u : 0u);
        h = HashFNV1a32(h, uint32_t(normal_compression.vertex_input_encoding));

        h = HashFNV1a32(h, normal_compression.compress_normal_map ? 1u : 0u);
        h = HashFNV1a32(h, uint32_t(normal_compression.normal_map_encoding));

        h = HashFNV1a32(h, normal_compression.compress_gbuffer_normal ? 1u : 0u);
        h = HashFNV1a32(h, uint32_t(normal_compression.gbuffer_encoding));

        return h;
    }

    inline uint32_t GBufferConfiguration::RecomputeVariantHash() const
    {
        return ComputeGBufferVariantHash(
            preset,
            level,
            channel_mask,
            enable_motion_vector,
            normal_compression);
    }

    struct RenderPassDefinition
    {
        RenderStage stage;
        bool depth_test;
        bool depth_write;
        GBufferChannel read_channels;
        GBufferChannel write_channels;
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
        Deferred_Standard,
        Deferred_Tiled,
        Deferred_Clustered,
        VisibilityBuffer_Deferred,
        Mobile_Forward,
        Mobile_SubpassDeferred,

        ENUM_CLASS_RANGE(Forward_Basic, Mobile_SubpassDeferred)
    };

    struct RenderFlowDefinition
    {
        const char *name;
        RenderFlowPreset preset;
        const RenderPassDefinition *passes;
        uint32_t pass_count;
        bool mobile_optimized;
        bool requires_compute_shader;
        GBufferQualityPreset min_required_quality;
        GBufferQualityPreset max_supported_quality;

        struct ResourceRequirement
        {
            uint32_t color_attachment_count;
            uint32_t depth_attachment_count;
            bool need_hzb_texture;
            bool need_shadow_map;
        } resource_requirements;

        VertexInputMigrationPolicy vertex_input_policy;
    };

    struct RenderPipeline
    {
        const RenderFlowDefinition *flow;
        const GBufferConfiguration *gbuffer_config;

        bool IsValid() const
        {
            return gbuffer_config->preset >= flow->min_required_quality
                && gbuffer_config->preset <= flow->max_supported_quality;
        }

        std::string GetSPVPath(RenderStage stage, const char *stage_suffix = nullptr) const;

        uint64_t GetConfigHash() const
        {
            return (uint64_t(flow->preset) << 32) | gbuffer_config->variant_hash;
        }
    };

    inline std::string RenderPipeline::GetSPVPath(RenderStage stage, const char *stage_suffix) const
    {
        const char *stage_name = [stage]() -> const char *
        {
            switch(stage)
            {
                case RenderStage::EarlyZ_Solid: return "EarlyZ_Solid";
                case RenderStage::EarlyZ_Masked: return "EarlyZ_Masked";
                case RenderStage::ShadowMap_Directional: return "ShadowMap_Directional";
                case RenderStage::ShadowMap_Spot: return "ShadowMap_Spot";
                case RenderStage::ShadowMap_Point: return "ShadowMap_Point";
                case RenderStage::GBuffer_Opaque: return "GBuffer_Opaque";
                case RenderStage::GBuffer_Masked: return "GBuffer_Masked";
                case RenderStage::VisibilityBuffer_Fill: return "VisibilityBuffer_Fill";
                case RenderStage::Deferred_Lighting: return "Deferred_Lighting";
                case RenderStage::Deferred_LightingTiled: return "Deferred_LightingTiled";
                case RenderStage::Deferred_LightingClustered: return "Deferred_LightingClustered";
                case RenderStage::Forward_Opaque: return "Forward_Opaque";
                case RenderStage::Forward_Masked: return "Forward_Masked";
                case RenderStage::Forward_Transparent: return "Forward_Transparent";
                case RenderStage::Forward_Additive: return "Forward_Additive";
                case RenderStage::HZB_Generation: return "HZB_Generation";
                case RenderStage::HZB_Culling: return "HZB_Culling";
                case RenderStage::PostProcess_TAA: return "PostProcess_TAA";
                case RenderStage::PostProcess_Bloom: return "PostProcess_Bloom";
                case RenderStage::PostProcess_ToneMapping: return "PostProcess_ToneMapping";
                case RenderStage::PostProcess_FXAA: return "PostProcess_FXAA";
                case RenderStage::PostProcess_MotionBlur: return "PostProcess_MotionBlur";
                case RenderStage::PostProcess_DOF: return "PostProcess_DOF";
                case RenderStage::PostProcess_SSR: return "PostProcess_SSR";
                case RenderStage::PostProcess_SSAO: return "PostProcess_SSAO";
                case RenderStage::Debug_Visualization: return "Debug_Visualization";
                default: return "Unknown";
            }
        }();

        char buffer[256];
        std::snprintf(buffer,
                      sizeof(buffer),
                      "%s_%s%s_%08X.spv",
                      flow->name,
                      stage_name,
                      stage_suffix ? stage_suffix : "",
                      gbuffer_config->variant_hash);

        return std::string(buffer);
    }
}//namespace hgl::graph::mtl
