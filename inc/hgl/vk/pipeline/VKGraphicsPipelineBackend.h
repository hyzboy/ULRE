#pragma once

#include<hgl/vk/pipeline/VKPipelineResolver.h>

namespace hgl::graph
{
    class GraphicsPipelineBackend
    {
    public:
        virtual ~GraphicsPipelineBackend() = default;

        virtual GraphicsPipelineMode GetMode() const = 0;
        virtual bool Materialize(const FinalPipelineResolveRequest &request, VkPipeline &out_pipeline) = 0;
        virtual void ClearCacheForDevice(VulkanDevice *device) = 0;

        static GraphicsPipelineBackend *Get(GraphicsPipelineMode mode);
        static uint64_t HashShaderStages(VulkanDevice *device,
                                         const ShaderStageCreateInfoList *shader_stages,
                                         VkShaderStageFlags stage_filter = 0);
        static uint64_t HashBytes(const void *data, size_t size);
    };
}
