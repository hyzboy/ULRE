#include<hgl/graph/manager/GraphManager.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
VkDevice GraphManager::GetVkDevice()
{
    return device->GetDevice();
}

const GPUPhysicalDevice *GraphManager::GetPhysicalDevice()const
{
    return device->GetPhysicalDevice();
}
VK_NAMESPACE_END
