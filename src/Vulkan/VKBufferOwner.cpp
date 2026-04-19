#include<hgl/vk/VKBufferOwner.h>
#include<hgl/vk/VKStagedBuffer.h>   // complete type for ~StagedBuffer via IGPUBuffer*
#include<hgl/vk/VKReBarBuffer.h>    // complete type for ~ReBarBuffer via IGPUBuffer*
#include<hgl/vk/VKDevice.h>

namespace hgl::graph{

VkBufferOwner::~VkBufferOwner()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(device);

    if(staged_source)
    {
        delete staged_source;
        staged_source = nullptr;
    }
    else
    {
        if(buf.buffer)
        {
            if(owner && buf.allocation)
                vmaDestroyBuffer(owner->GetVmaAllocator(), buf.buffer, buf.allocation);
            else
                vkDestroyBuffer(device, buf.buffer, nullptr);
        }
    }

    if(owner)
    {
        if(buf.buffer)
            owner->UntrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)buf.buffer);

        if(buf.vk_memory)
            owner->UntrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)buf.vk_memory);
    }
}

}//namespace hgl::graph
