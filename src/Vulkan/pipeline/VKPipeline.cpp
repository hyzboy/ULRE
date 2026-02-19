#include<hgl/vk/pipeline/VKPipeline.h>
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
        GLogDebug("[Pipeline::~Pipeline]   UntrackObject called for Pipeline '%s'", name.c_str());
        owner->UntrackObject(VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)pipeline);
    }
    else
    {
        GLogWarning("[Pipeline::~Pipeline]   WARNING: VulkanDevice owner is NULL for Pipeline '%s'", name.c_str());
    }

    delete data;
    GLogDebug("[Pipeline::~Pipeline]   Calling vkDestroyPipeline for '%s'", name.c_str());
    vkDestroyPipeline(device,pipeline,nullptr);
    GLogDebug("[Pipeline::~Pipeline]   Destroyed Pipeline '%s'", name.c_str());
}
}//namespace hgl::graph
