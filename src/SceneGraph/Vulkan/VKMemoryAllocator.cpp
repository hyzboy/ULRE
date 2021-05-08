#include<hgl/graph/VKMemoryAllocator.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKBuffer.h>

VK_NAMESPACE_BEGIN
VKMemoryAllocator::VKMemoryAllocator(GPUDevice *d,const uint32_t flags)
{
    device=d;
    buffer_usage_flag_bits=flags;
    gpu_buffer=nullptr;

    const GPUPhysicalDevice *pd=device->GetPhysicalDevice();

    SetAllocUnitSize(pd->GetUBOAlign());
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

    gpu_buffer=device->CreateBuffer(buffer_usage_flag_bits,alloc_size);

    if(!gpu_buffer)
    {
        memory_block=nullptr;
        return(false);
    }

    memory_block=gpu_buffer->Map();

    return(true);
}
VK_NAMESPACE_END
