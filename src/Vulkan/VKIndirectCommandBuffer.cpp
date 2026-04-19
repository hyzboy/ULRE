#include<hgl/vk/VKIndirectCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKStagedBuffer.h>
#include<hgl/vk/VKReBarBuffer.h>
#include<hgl/object/ObjectTracker.h>

namespace hgl::graph{

bool VulkanDevice::CreateIndirectCommandBuffer(DeviceBufferData *buf,const uint32_t cmd_count,const uint32_t cmd_size,const ObjectNameBuilder &name,SharingMode sharing_mode)
{
    HGL_CAPTURE_SCOPE();

    const uint32_t size=cmd_count*cmd_size;

    if(size<=0)return(false);

    return CreateBuffer(buf,VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,size,size,nullptr,sharing_mode,name);
}

bool VulkanDevice::CreateIndirectCommandBuffer(DeviceBufferData *buf,const uint32_t cmd_count,const uint32_t cmd_size,BufferAllocPolicy policy,IGPUBuffer **staged_out,const ObjectNameBuilder &name,SharingMode sharing_mode)
{
    HGL_CAPTURE_SCOPE();

    if(staged_out)
        *staged_out=nullptr;

    const uint32_t size=cmd_count*cmd_size;
    if(size<=0)return(false);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(name, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, size, nullptr, sharing_mode);
        if(!staged)
            return(false);

        buf->buffer=staged->GetVkDeviceBuffer();
        buf->allocation=VK_NULL_HANDLE;
        buf->vk_memory=static_cast<VkDeviceMemory>(*staged->GetDeviceMemory());
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

    if(!CreateBuffer(buf,VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,size,size,nullptr,sharing_mode,mem_usage,name))
        return(false);

    if(staged_out)
    {
        const std::string buf_name = name.base_name[0] ? std::string(name.base_name) : std::string("IndirectBuffer");
        *staged_out = new ReBarBuffer(buf_name, vma_allocator, buf->buffer, buf->allocation, size);
    }

    return(true);
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
    IGPUBuffer *staged=nullptr;

    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDrawIndirectCommand),policy,&staged,name,sm))
        return(nullptr);

    if(staged)
    {
        auto *buf_obj = new IndirectDrawBuffer(attr->device,buf,cmd_count);
        buf_obj->SetStagedSource(staged);
        return buf_obj;
    }

    return(new IndirectDrawBuffer(attr->device,buf,cmd_count));
}

IndirectDrawIndexedBuffer *VulkanDevice::CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,const ObjectNameBuilder &name,SharingMode sm)
{
    return CreateIndirectDrawIndexedBuffer(cmd_count,BufferAllocPolicy::Auto,name,sm);
}

IndirectDrawIndexedBuffer *VulkanDevice::CreateIndirectDrawIndexedBuffer(const uint32_t cmd_count,BufferAllocPolicy policy,const ObjectNameBuilder &name,SharingMode sm)
{
    HGL_CAPTURE_SCOPE();
    DeviceBufferData buf;
    IGPUBuffer *staged=nullptr;

    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDrawIndexedIndirectCommand),policy,&staged,name,sm))
        return(nullptr);

    if(staged)
    {
        auto *buf_obj = new IndirectDrawIndexedBuffer(attr->device,buf,cmd_count);
        buf_obj->SetStagedSource(staged);
        return buf_obj;
    }

    return(new IndirectDrawIndexedBuffer(attr->device,buf,cmd_count));
}

IndirectDispatchBuffer *VulkanDevice::CreateIndirectDispatchBuffer(const uint32_t cmd_count,const ObjectNameBuilder &name,SharingMode sm)
{
    return CreateIndirectDispatchBuffer(cmd_count,BufferAllocPolicy::Auto,name,sm);
}

IndirectDispatchBuffer *VulkanDevice::CreateIndirectDispatchBuffer(const uint32_t cmd_count,BufferAllocPolicy policy,const ObjectNameBuilder &name,SharingMode sm)
{
    HGL_CAPTURE_SCOPE();
    DeviceBufferData buf;
    IGPUBuffer *staged=nullptr;

    if(!CreateIndirectCommandBuffer(&buf,cmd_count,sizeof(VkDispatchIndirectCommand),policy,&staged,name,sm))
        return(nullptr);

    if(staged)
    {
        auto *buf_obj = new IndirectDispatchBuffer(attr->device,buf,cmd_count);
        buf_obj->SetStagedSource(staged);
        return buf_obj;
    }

    return(new IndirectDispatchBuffer(attr->device,buf,cmd_count));
}

}//namespace hgl::graph
