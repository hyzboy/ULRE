#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKImageView.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKSurface.h>
#include<hgl/vk/VKDevice.h>
#include<iostream>

namespace hgl::graph{
void SavePipelineCacheData(VkDevice device,VkPipelineCache cache,const VkPhysicalDeviceProperties &pdp);

VulkanDevAttr::VulkanDevAttr(VulkanInstance *inst,const VulkanPhyDevice *pd,VulkanSurface *s)
{
    instance=inst;
    physical_device=pd;
    surface=s;
}

VulkanDevAttr::~VulkanDevAttr()
{
#ifdef _DEBUG
    if(debug_utils)
        delete debug_utils;
#endif//_DEBUG

    if(pipeline_cache)
    {
        SavePipelineCacheData(device,pipeline_cache,physical_device->GetProperties());
        vkDestroyPipelineCache(device,pipeline_cache,nullptr);
    }

    if(desc_pool)
        vkDestroyDescriptorPool(device,desc_pool,nullptr);

    if(cmd_pool)
    {
        VulkanDevice *owner = VulkanDevice::FromDevice(device);
        if (owner)
            owner->UntrackObject(VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)(uintptr_t)cmd_pool);
        vkDestroyCommandPool(device,cmd_pool,nullptr);
    }

    if(device)
        vkDestroyDevice(device,nullptr);

    if(surface)
        delete surface;
}

int VulkanDevAttr::GetMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties) const
{
    return physical_device->GetMemoryType(typeBits,properties);
}

}//namespace hgl::graph
