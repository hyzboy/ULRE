#include<hgl/vk/VKFence.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::graph{

Fence::Fence(VkDevice d,VkFence f,const OSString &fence_name)
    :Object()
    ,device(d)
    ,fence(f)
{
    SetInstanceName(fence_name);
}

Fence::~Fence()
{
    Release();                  //幂等：OnRelease() 完成 VkFence 销毁
}

void Fence::OnRelease()
{
    if(fence==VK_NULL_HANDLE)
        return;

    VulkanDevice *owner = VulkanDevice::FromDevice(device);

    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_FENCE, (uint64_t)(uintptr_t)fence);

    vkDestroyFence(device, fence, nullptr);

    fence=VK_NULL_HANDLE;
}

}//namespace hgl::graph
