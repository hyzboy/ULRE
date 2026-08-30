#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<hgl/type/String.h>
#include<cstdint>

namespace hgl::graph
{
    struct FrameOutputConfig
    {
        const VkFormat *color_formats = nullptr;
        uint32_t color_attachment_count = 0;
        VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
    };

    /**
     * 最终管线 key——pipeline 只保留 shader 部分（全动态状态走 EDS 1/2/3，
     * 渲染侧 vkCmdSet* 应用材质配置），故 key 仅含 shader stage + 帧输出附件格式。
     */
    struct FinalPipelineKey
    {
        uint64_t shader_stages_hash = 0;        ///< shader stage（stage 位 + module + entry point）
        uint64_t color_formats_hash = 0;        ///< 颜色附件格式集合
        VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
        uint32_t color_attachment_count = 0;

        bool operator == (const FinalPipelineKey &rhs) const
        {
            return shader_stages_hash == rhs.shader_stages_hash
                && color_formats_hash == rhs.color_formats_hash
                && depth_stencil_format == rhs.depth_stencil_format
                && color_attachment_count == rhs.color_attachment_count;
        }
    };

    class VulkanDevice;

    struct FinalPipelineResolveRequest
    {
        VulkanDevice *device = nullptr;
        VkPipelineCache pipeline_cache = VK_NULL_HANDLE;

        FrameOutputConfig frame_output{};

        const AnsiString *debug_name = nullptr;
        const ShaderStageCreateInfoList *shader_stages = nullptr;
        VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    };

    struct FinalPipelineResolveResult
    {
        FinalPipelineKey key{};
        VkPipeline pipeline = VK_NULL_HANDLE;
    };

    class PipelineResolver
    {
    public:
        static bool BuildFinalPipelineKey(const FinalPipelineResolveRequest &request, FinalPipelineKey &out_key);
        static bool HasCompleteFinalKey(const FinalPipelineKey &key);
        static bool MaterializePipeline(const FinalPipelineResolveRequest &request, VkPipeline &out_pipeline);
        static bool ResolveFinalPipeline(const FinalPipelineResolveRequest &request, FinalPipelineResolveResult &out_result);

        /// Release a final pipeline reference held by the resolver cache.
        /// The caller remains responsible for destroying the VkPipeline handle.
        static void ReleaseFinalPipeline(VulkanDevice *device, VkPipeline pipeline);

        /// Remove all cached pipelines created for the given device.
        /// Must be called before the device is destroyed.
        static void ClearCacheForDevice(VulkanDevice *device);

    };
}//namespace hgl::graph
