#include<hgl/vk/VKFence.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>
namespace hgl::graph{
Fence::~Fence()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_FENCE, (uint64_t)(uintptr_t)fence);

    vkDestroyFence(device,fence,nullptr);
}
}//namespace hgl::graph
