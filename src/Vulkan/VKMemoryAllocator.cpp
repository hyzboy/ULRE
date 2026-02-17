#include<hgl/vk/VKMemoryAllocator.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKBuffer.h>

namespace hgl::graph{
VKMemoryAllocator::VKMemoryAllocator(VulkanDevice *d,const uint32_t flags,const VkDeviceSize r)
{
    device=d;
    buffer_usage_flag_bits=flags;
    gpu_buffer=nullptr;
    range=r;

    SetAllocUnitSize(range);
}

VKMemoryAllocator::~VKMemoryAllocator()
{
    if(gpu_buffer)
    {
        gpu_buffer->Unmap();
        delete gpu_buffer;
    }
}

bool VKMemoryAllocator::AllocMemory()
{
    if(gpu_buffer)
        delete gpu_buffer;

    gpu_buffer=device->CreateBuffer(buffer_usage_flag_bits,range,alloc_size);

    if(!gpu_buffer)
    {
        memory_block=nullptr;
        return(false);
    }

    memory_block=gpu_buffer->Map();

    return(true);
}

void VKMemoryAllocator::Flush(const VkDeviceSize size)
{
    gpu_buffer->Flush(size);
}

bool VKMemoryAllocator::Write(const void *source,const uint64 offset,const uint64 size)
{
    if(!source||size==0)
        return(false);

    return gpu_buffer->Write(source,offset,size);
}
}//namespace hgl::graph
