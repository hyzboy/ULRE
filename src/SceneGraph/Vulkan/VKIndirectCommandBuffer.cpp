#include<hgl/graph/VKIndirectCommandBuffer.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

bool GPUDevice::CreateIndirectCommandBuffer(DeviceBufferData *buf,const uint32_t cmd_count,const uint32_t cmd_size,SharingMode sharing_mode)
{
    const uint32_t size=cmd_count*cmd_size;

    if(size<=0)return(false);

    return CreateBuffer(buf,VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,size,size,nullptr,sharing_mode);
}

IndirectDrawBuffer *GPUDevice::CreateIndirectDrawBuffer(const uint32_t cmd_count,SharingMode sm)
{
    DeviceBufferData buf;
        
    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDrawIndirectCommand),sm))
        return(nullptr);

    return(new IndirectDrawBuffer(attr->device,buf,cmd_count));
}

IndirectDrawIndexedBuffer *GPUDevice::CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,SharingMode sm)
{
    DeviceBufferData buf;
        
    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDrawIndexedIndirectCommand),sm))
        return(nullptr);

    return(new IndirectDrawIndexedBuffer(attr->device,buf,cmd_count));
}

IndirectDispatchBuffer *GPUDevice::CreateIndirectDispatchBuffer(const uint32_t cmd_count,SharingMode sm)
{
    DeviceBufferData buf;
        
    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDispatchIndirectCommand),sm))
        return(nullptr);

    return(new IndirectDispatchBuffer(attr->device,buf,cmd_count));
}

VK_NAMESPACE_END
