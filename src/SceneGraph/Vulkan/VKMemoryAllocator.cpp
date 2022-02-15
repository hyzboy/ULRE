#include<hgl/graph/VKMemoryAllocator.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKBuffer.h>

VK_NAMESPACE_BEGIN
VKMemoryAllocator::VKMemoryAllocator(GPUDevice *d,const uint32_t flags,const VkDeviceSize r)
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
VK_NAMESPACE_END
