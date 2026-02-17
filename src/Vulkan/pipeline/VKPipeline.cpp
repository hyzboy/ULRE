#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>
#include<iostream>

namespace hgl::graph{
Pipeline::~Pipeline()
{
    std::cout << "[Pipeline::~Pipeline] Destroying Pipeline '" << name << "' (VkPipeline=0x" << std::hex 
              << (uintptr_t)pipeline << std::dec << ", Pipeline*=0x" << (uintptr_t)this << ")" << std::endl;
    
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
    {
        std::cout << "[Pipeline::~Pipeline]   UntrackObject called for Pipeline '" << name << "'" << std::endl;
        owner->UntrackObject(VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)pipeline);
    }
    else
    {
        std::cout << "[Pipeline::~Pipeline]   WARNING: VulkanDevice owner is NULL for Pipeline '" << name << "'" << std::endl;
    }

    delete data;
    std::cout << "[Pipeline::~Pipeline]   Calling vkDestroyPipeline for '" << name << "'" << std::endl;
    vkDestroyPipeline(device,pipeline,nullptr);
    std::cout << "[Pipeline::~Pipeline]   Destroyed Pipeline '" << name << "'" << std::endl;
}
}//namespace hgl::graph
