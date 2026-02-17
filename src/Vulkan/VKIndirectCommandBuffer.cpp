#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::graph{

bool VulkanDevice::CreateIndirectCommandBuffer(DeviceBufferData *buf,const uint32_t cmd_count,const uint32_t cmd_size,const ObjectNameBuilder &name,SharingMode sharing_mode)
{
    const uint32_t size=cmd_count*cmd_size;

    if(size<=0)return(false);

    return CreateBuffer(buf,VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,size,size,nullptr,sharing_mode,name);
}

bool VulkanDevice::CreateIndirectCommandBuffer(DeviceBufferData *buf,const uint32_t cmd_count,const uint32_t cmd_size,BufferAllocPolicy policy,StagedBuffer **staged_out,const ObjectNameBuilder &name,SharingMode sharing_mode)
{
    if(staged_out)
        *staged_out=nullptr;

    const uint32_t size=cmd_count*cmd_size;
    if(size<=0)return(false);

    if(policy==BufferAllocPolicy::Auto)
    {
        if(attr->physical_device->HasReBAR())
            policy=BufferAllocPolicy::CPUVisible;
        else
            policy=BufferAllocPolicy::StagedUpload;
    }

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,size,nullptr,sharing_mode);
        if(!staged)
            return(false);

        buf->buffer=staged->GetDeviceBuffer();
        buf->memory=staged->GetDeviceMemory();
        buf->info.buffer=buf->buffer;
        buf->info.offset=0;
        buf->info.range=size;

        if(staged_out)
            *staged_out=staged;

        return(true);
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    return CreateBuffer(buf,VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,size,size,nullptr,sharing_mode,mem_usage,name);
}

IndirectDrawBuffer *VulkanDevice::CreateIndirectDrawBuffer(const uint32_t cmd_count,SharingMode sm)
{
    return CreateIndirectDrawBuffer(cmd_count,BufferAllocPolicy::Auto,sm);
}

IndirectDrawBuffer *VulkanDevice::CreateIndirectDrawBuffer(const uint32_t cmd_count,BufferAllocPolicy policy,SharingMode sm)
{
    DeviceBufferData buf;
    StagedBuffer *staged=nullptr;

    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDrawIndirectCommand),policy,&staged,VK_NAME_FROM("IndirectDrawBuffer:Memory"),sm))
        return(nullptr);

    if(staged)
        return(new IndirectDrawBuffer(this,attr->device,buf,cmd_count,staged));

    return(new IndirectDrawBuffer(this,attr->device,buf,cmd_count));
}

IndirectDrawIndexedBuffer *VulkanDevice::CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,SharingMode sm)
{
    return CreateIndirectDrawIndexedBuffer(cmd_count,BufferAllocPolicy::Auto,sm);
}

IndirectDrawIndexedBuffer *VulkanDevice::CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,BufferAllocPolicy policy,SharingMode sm)
{
    DeviceBufferData buf;
    StagedBuffer *staged=nullptr;

    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDrawIndexedIndirectCommand),policy,&staged,VK_NAME_FROM("IndirectDrawIndexedBuffer:Memory"),sm))
        return(nullptr);

    if(staged)
        return(new IndirectDrawIndexedBuffer(this,attr->device,buf,cmd_count,staged));

    return(new IndirectDrawIndexedBuffer(this,attr->device,buf,cmd_count));
}

IndirectDispatchBuffer *VulkanDevice::CreateIndirectDispatchBuffer(const uint32_t cmd_count,SharingMode sm)
{
    return CreateIndirectDispatchBuffer(cmd_count,BufferAllocPolicy::Auto,sm);
}

IndirectDispatchBuffer *VulkanDevice::CreateIndirectDispatchBuffer(const uint32_t cmd_count,BufferAllocPolicy policy,SharingMode sm)
{
    DeviceBufferData buf;
    StagedBuffer *staged=nullptr;

    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDispatchIndirectCommand),policy,&staged,VK_NAME_FROM("IndirectDispatchBuffer:Memory"),sm))
        return(nullptr);

    if(staged)
        return(new IndirectDispatchBuffer(this,attr->device,buf,cmd_count,staged));

    return(new IndirectDispatchBuffer(this,attr->device,buf,cmd_count));
}

}//namespace hgl::graph
