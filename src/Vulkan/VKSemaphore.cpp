#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKDevice.h>
VK_NAMESPACE_BEGIN
Semaphore::~Semaphore()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)(uintptr_t)sem);

    vkDestroySemaphore(device,sem,nullptr);
}
VK_NAMESPACE_END
