#include<hgl/vk/VKSampler.h>
#include<hgl/vk/VKDevice.h>
namespace hgl::graph{
Sampler::~Sampler()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_SAMPLER, (uint64_t)(uintptr_t)sampler);

    vkDestroySampler(device,sampler,nullptr);
}
}//namespace hgl::graph
