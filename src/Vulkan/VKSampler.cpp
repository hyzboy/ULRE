#include<hgl/vk/VKSampler.h>
#include<hgl/vk/VKDevice.h>
VK_NAMESPACE_BEGIN
Sampler::~Sampler()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_SAMPLER, (uint64_t)(uintptr_t)sampler);

    vkDestroySampler(device,sampler,nullptr);
}
VK_NAMESPACE_END
