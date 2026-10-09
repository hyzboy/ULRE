#include<hgl/vk/pipeline/VKShaderObjectPipelineBackend.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{
    ShaderObjectPipelineBackend &ShaderObjectPipelineBackend::Instance()
    {
        static ShaderObjectPipelineBackend backend;
        return backend;
    }

    GraphicsPipelineMode ShaderObjectPipelineBackend::GetMode() const
    {
        return GraphicsPipelineMode::ShaderObject;
    }

    bool ShaderObjectPipelineBackend::Materialize(const FinalPipelineResolveRequest &, VkPipeline &out_pipeline)
    {
        out_pipeline = VK_NULL_HANDLE;
        GLogError("[PipelineResolver] Shader Object backend is not implemented");
        return false;
    }

    void ShaderObjectPipelineBackend::ClearCacheForDevice(VulkanDevice *)
    {
    }
}
