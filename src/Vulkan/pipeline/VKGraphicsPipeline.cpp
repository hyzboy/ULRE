#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>
#include<hgl/log/Log.h>

namespace hgl::graph{
GraphicsPipeline::~GraphicsPipeline()
{
    GLogDebug("[GraphicsPipeline::~GraphicsPipeline] Destroying GraphicsPipeline '%s' (VkPipeline=0x%llx, GraphicsPipeline*=0x%llx)",
              name.c_str(), (unsigned long long)(uintptr_t)pipeline, (unsigned long long)(uintptr_t)this);

    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
    {
        GLogDebug("[GraphicsPipeline::~GraphicsPipeline]   UntrackObject called for GraphicsPipeline '%s'", name.c_str());
        owner->UntrackObject(VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)pipeline);
    }
    else
    {
        GLogWarning("[GraphicsPipeline::~GraphicsPipeline]   WARNING: VulkanDevice owner is NULL for GraphicsPipeline '%s'", name.c_str());
    }

    delete data;
    GLogDebug("[GraphicsPipeline::~GraphicsPipeline]   Calling vkDestroyPipeline for '%s'", name.c_str());
    vkDestroyPipeline(device,pipeline,nullptr);
    GLogDebug("[GraphicsPipeline::~GraphicsPipeline]   Destroyed GraphicsPipeline '%s'", name.c_str());
}
}//namespace hgl::graph
