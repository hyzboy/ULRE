#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/object/ObjectTracker.h>

namespace hgl::graph{

bool VulkanDevice::CreateIndirectCommandBuffer(DeviceBufferData *buf,const uint32_t cmd_count,const uint32_t cmd_size,const ObjectNameBuilder &name,SharingMode sharing_mode)
{
    HGL_CAPTURE_SCOPE();

    const uint32_t size=cmd_count*cmd_size;

    if(size<=0)return(false);

    return CreateBuffer(buf,VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,size,size,nullptr,sharing_mode,name);
}

bool VulkanDevice::CreateIndirectCommandBuffer(DeviceBufferData *buf,const uint32_t cmd_count,const uint32_t cmd_size,BufferAllocPolicy policy,StagedBuffer **staged_out,const ObjectNameBuilder &name,SharingMode sharing_mode)
{
    HGL_CAPTURE_SCOPE();

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
        StagedBuffer *staged=CreateStagedBuffer(name, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, size, nullptr, sharing_mode);
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

// 新版本：带名字追踪
IndirectDrawBuffer *VulkanDevice::CreateIndirectDrawBuffer(const uint32_t cmd_count,const ObjectNameBuilder &name,SharingMode sm)
{
    return CreateIndirectDrawBuffer(cmd_count,BufferAllocPolicy::Auto,name,sm);
}

IndirectDrawBuffer *VulkanDevice::CreateIndirectDrawBuffer(const uint32_t cmd_count,BufferAllocPolicy policy,const ObjectNameBuilder &name,SharingMode sm)
{
    HGL_CAPTURE_SCOPE();
    DeviceBufferData buf;
    StagedBuffer *staged=nullptr;

    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDrawIndirectCommand),policy,&staged,name,sm))
        return(nullptr);

    if(staged)
        return(new IndirectDrawBuffer(this,attr->device,buf,cmd_count,staged));

    return(new IndirectDrawBuffer(this,attr->device,buf,cmd_count));
}

IndirectDrawIndexedBuffer *VulkanDevice::CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,const ObjectNameBuilder &name,SharingMode sm)
{
    return CreateIndirectDrawIndexedBuffer(cmd_count,BufferAllocPolicy::Auto,name,sm);
}

IndirectDrawIndexedBuffer *VulkanDevice::CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,BufferAllocPolicy policy,const ObjectNameBuilder &name,SharingMode sm)
{
    HGL_CAPTURE_SCOPE();
    DeviceBufferData buf;
    StagedBuffer *staged=nullptr;

    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDrawIndexedIndirectCommand),policy,&staged,name,sm))
        return(nullptr);

    if(staged)
        return(new IndirectDrawIndexedBuffer(this,attr->device,buf,cmd_count,staged));

    return(new IndirectDrawIndexedBuffer(this,attr->device,buf,cmd_count));
}

IndirectDispatchBuffer *VulkanDevice::CreateIndirectDispatchBuffer(const uint32_t cmd_count,const ObjectNameBuilder &name,SharingMode sm)
{
    return CreateIndirectDispatchBuffer(cmd_count,BufferAllocPolicy::Auto,name,sm);
}

IndirectDispatchBuffer *VulkanDevice::CreateIndirectDispatchBuffer(const uint32_t cmd_count,BufferAllocPolicy policy,const ObjectNameBuilder &name,SharingMode sm)
{
    HGL_CAPTURE_SCOPE();
    DeviceBufferData buf;
    StagedBuffer *staged=nullptr;

    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDispatchIndirectCommand),policy,&staged,name,sm))
        return(nullptr);

    if(staged)
        return(new IndirectDispatchBuffer(this,attr->device,buf,cmd_count,staged));

    return(new IndirectDispatchBuffer(this,attr->device,buf,cmd_count));
}

}//namespace hgl::graph
