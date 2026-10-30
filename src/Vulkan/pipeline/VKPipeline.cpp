#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/pipeline/VKPipelineResolver.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>
#include<hgl/log/Log.h>

namespace hgl::graph{
Pipeline::~Pipeline()
{
    GLogDebug("[Pipeline::~Pipeline] Destroying Pipeline '%s' (VkPipeline=0x%llx, Pipeline*=0x%llx)",
              name.c_str(), (unsigned long long)(uintptr_t)pipeline, (unsigned long long)(uintptr_t)this);

    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
    {
        const PipelineReleaseResult result = PipelineResolver::ReleaseFinalPipeline(owner, pipeline);
        if (result == PipelineReleaseResult::Destroyed)
        {
            GLogDebug("[Pipeline::~Pipeline]   VkPipeline 0x%llx reached ref 0 and was destroyed",
                      (unsigned long long)(uintptr_t)pipeline);
        }
        else if (result == PipelineReleaseResult::StillReferenced)
        {
            GLogDebug("[Pipeline::~Pipeline]   VkPipeline 0x%llx still referenced by other Pipeline wrappers",
                      (unsigned long long)(uintptr_t)pipeline);
        }
        else // NotFound
        {
            owner->UntrackObject(VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)pipeline);
            vkDestroyPipeline(device, pipeline, nullptr);
            GLogDebug("[Pipeline::~Pipeline]   Destroyed unmanaged Pipeline '%s'", name.c_str());
        }
    }
    else
    {
        GLogWarning("[Pipeline::~Pipeline]   WARNING: VulkanDevice owner is NULL for Pipeline '%s', destroying handle directly", name.c_str());
        vkDestroyPipeline(device, pipeline, nullptr);
    }
}
}//namespace hgl::graph
