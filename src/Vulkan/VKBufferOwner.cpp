#include<hgl/vk/VKBufferOwner.h>
#include<hgl/vk/VKStagedBuffer.h>   // complete type for ~StagedBuffer via IGPUBuffer*
#include<hgl/vk/VKReBarBuffer.h>    // complete type for ~ReBarBuffer via IGPUBuffer*
#include<hgl/vk/VKDevice.h>

namespace hgl::graph{

VkBufferOwner::~VkBufferOwner()
{
    if(staged_source)
    {
        delete staged_source;
        staged_source = nullptr;
    }
    else
    {
        if(buf.buffer)
        {
            VulkanDevice *owner = VulkanDevice::FromDevice(device);
            if(owner && buf.allocation)
                vmaDestroyBuffer(owner->GetVmaAllocator(), buf.buffer, buf.allocation);
            else
                vkDestroyBuffer(device, buf.buffer, nullptr);
        }
    }
}

}//namespace hgl::graph
