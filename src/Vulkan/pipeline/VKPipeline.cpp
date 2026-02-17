#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>

namespace hgl::graph{
Pipeline::~Pipeline()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)pipeline);

    delete data;
    vkDestroyPipeline(device,pipeline,nullptr);
}
}//namespace hgl::graph
