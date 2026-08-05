#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<hgl/graph/PipelinePreset.h>
#include<hgl/type/String.h>
#include<cstdint>

namespace hgl::graph
{
    class GeometryVertexFormat;

    enum class PipelineMaterializeMode:uint8
    {
        Monolithic = 0,
        GraphicsPipelineLibrary
    };

    struct VertexInterfaceKey
    {
        uint64_t format_hash = 0;
        uint32_t binding_count = 0;
        uint32_t attribute_count = 0;

        bool operator == (const VertexInterfaceKey &rhs) const
        {
            return format_hash == rhs.format_hash
                && binding_count == rhs.binding_count
                && attribute_count == rhs.attribute_count;
        }
    };

    struct PreRasterPipelineKey
    {
        uint64_t shader_program_hash = 0;
        uint64_t config_hash = 0;
        PrimitiveType primitive_type = PrimitiveType::Triangles;

        bool operator == (const PreRasterPipelineKey &rhs) const
        {
            return shader_program_hash == rhs.shader_program_hash
                && config_hash == rhs.config_hash
                && primitive_type == rhs.primitive_type;
        }
    };

    struct FragmentShaderKey
    {
        uint64_t shader_program_hash = 0;
        uint64_t variant_hash = 0;

        bool operator == (const FragmentShaderKey &rhs) const
        {
            return shader_program_hash == rhs.shader_program_hash
                && variant_hash == rhs.variant_hash;
        }
    };

    struct FragmentOutputKey
    {
        uint64_t color_formats_hash = 0;
        uint64_t output_state_hash = 0;
        VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
        uint32_t sample_count = 1;
        uint32_t color_attachment_count = 0;

        bool operator == (const FragmentOutputKey &rhs) const
        {
            return color_formats_hash == rhs.color_formats_hash
                && output_state_hash == rhs.output_state_hash
                && depth_stencil_format == rhs.depth_stencil_format
                && sample_count == rhs.sample_count
                && color_attachment_count == rhs.color_attachment_count;
        }
    };

    struct FrameOutputConfig
    {
        const VkFormat *color_formats = nullptr;
        uint32_t color_attachment_count = 0;
        VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
    };

    struct FinalPipelineKey
    {
        PipelinePreset preset = PipelinePreset::Auto;
        VertexInterfaceKey vi;
        PreRasterPipelineKey pr;
        FragmentShaderKey fs;
        FragmentOutputKey fo;

        bool operator == (const FinalPipelineKey &rhs) const
        {
            return preset == rhs.preset
                && vi == rhs.vi
                && pr == rhs.pr
                && fs == rhs.fs
                && fo == rhs.fo;
        }
    };

    struct PipelineCapabilityInfo
    {
        bool graphics_pipeline_library = false;
        PipelineMaterializeMode preferred_materialize_mode = PipelineMaterializeMode::Monolithic;
    };

    class VulkanDevice;
    class VulkanPhyDevice;

    struct FinalPipelineResolveRequest
    {
        VulkanDevice *device = nullptr;
        VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
        VkRenderPass render_pass = VK_NULL_HANDLE;
        uint32_t subpass = 0;

        FrameOutputConfig frame_output{};

        const AnsiString *debug_name = nullptr;
        PipelinePreset pipeline_preset = PipelinePreset::Auto;
        PipelineData *pipeline_data = nullptr;
        const ShaderStageCreateInfoList *shader_stages = nullptr;
        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
        const VIL *vertex_input_layout = nullptr;
        const GeometryVertexFormat *geometry_vertex_format = nullptr;
    };

    struct FinalPipelineResolveResult
    {
        FinalPipelineKey key{};
        PipelineCapabilityInfo capability{};
        PipelineMaterializeMode materialize_mode = PipelineMaterializeMode::Monolithic;
        VkPipeline pipeline = VK_NULL_HANDLE;
    };

    struct PipelineResolverQueryStats
    {
        uint64_t requests                = 0;
        uint64_t invalid_request         = 0;
        uint64_t incomplete_key          = 0;
        uint64_t fo_mismatch             = 0;
        uint64_t final_cache_hit         = 0;
        uint64_t final_cache_miss        = 0;
        uint64_t materialize_success     = 0;
        uint64_t materialize_failed      = 0;
        uint64_t vi_library_hit          = 0;
        uint64_t vi_library_miss         = 0;
        uint64_t pr_library_hit          = 0;
        uint64_t pr_library_miss         = 0;
        uint64_t fs_library_hit          = 0;
        uint64_t fs_library_miss         = 0;
        uint64_t fo_library_hit          = 0;
        uint64_t fo_library_miss         = 0;
        uint32_t monolithic_cache_entries = 0;
        uint32_t gpl_cache_entries        = 0;
        uint32_t vi_library_entries       = 0;
        uint32_t pr_library_entries       = 0;
        uint32_t fs_library_entries       = 0;
        uint32_t fo_library_entries       = 0;
    };

    class PipelineResolver
    {
    public:
        static PipelineCapabilityInfo BuildCapabilityInfo(const VulkanPhyDevice *physical_device);
        static PipelineMaterializeMode ResolveMaterializeMode(const VulkanPhyDevice *physical_device);
        static bool BuildFinalPipelineKey(const FinalPipelineResolveRequest &request, FinalPipelineKey &out_key);
        static bool HasCompleteFinalKey(const FinalPipelineKey &key);
        static bool MaterializeMonolithic(const FinalPipelineResolveRequest &request, VkPipeline &out_pipeline);
        static bool ResolveFinalPipeline(const FinalPipelineResolveRequest &request, FinalPipelineResolveResult &out_result);

        /// Remove all cached pipelines created for the given device.
        /// Must be called before the device is destroyed.
        static void ClearCacheForDevice(VulkanDevice *device);

        /// Query accumulated resolver statistics (thread-unsafe snapshot).
        static PipelineResolverQueryStats QueryStats();
    };
}//namespace hgl::graph
