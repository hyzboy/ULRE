#pragma once

#include<hgl/vk/pipeline/VKGraphicsPipelineBackend.h>

namespace hgl::graph
{
    class ShaderObjectPipelineBackend final : public GraphicsPipelineBackend
    {
    public:
        static ShaderObjectPipelineBackend &Instance();

        GraphicsPipelineMode GetMode() const override;
        bool Materialize(const FinalPipelineResolveRequest &request, VkPipeline &out_pipeline) override;
        void ClearCacheForDevice(VulkanDevice *device) override;
    };
}
