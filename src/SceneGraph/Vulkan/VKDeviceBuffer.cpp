#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<iostream>

VK_NAMESPACE_BEGIN
const VkDeviceSize VulkanDevice::GetUBOAlign   (){return attr->physical_device->GetUBOAlign();}
const VkDeviceSize VulkanDevice::GetSSBOAlign  (){return attr->physical_device->GetSSBOAlign();}
const VkDeviceSize VulkanDevice::GetUBORange   (){return attr->physical_device->GetUBORange();}
const VkDeviceSize VulkanDevice::GetSSBORange  (){return attr->physical_device->GetSSBORange();}

bool VulkanDevice::CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode)
{
    return CreateBuffer(buf,buf_usage,range,size,data,sharing_mode,MemoryUsage::CPUOnly);
}

bool VulkanDevice::CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,MemoryUsage mem_usage)
{
    if(size<=0)return(false);

    BufferCreateInfo buf_info;

    buf_info.usage                  = buf_usage;
    buf_info.size                   = size;
    buf_info.queueFamilyIndexCount  = 0;
    buf_info.pQueueFamilyIndices    = nullptr;
    buf_info.sharingMode            = VkSharingMode(sharing_mode);

    if(vkCreateBuffer(attr->device,&buf_info,nullptr,&buf->buffer)!=VK_SUCCESS)
        return(false);

    VkMemoryRequirements mem_reqs;

    vkGetBufferMemoryRequirements(attr->device,buf->buffer,&mem_reqs);

    DeviceMemory *dm=CreateMemory(mem_reqs,mem_usage);

    if(dm&&dm->BindBuffer(buf->buffer))
    {
        buf->info.buffer  =buf->buffer;
        buf->info.offset  =0;
        buf->info.range   =range;

        buf->memory       =dm;

        if(!data)
            return(true);

        dm->Write(data,0,size);
        return(true);
    }

    delete dm;

    vkDestroyBuffer(attr->device,buf->buffer,nullptr);
    return(false);
}

VAB *VulkanDevice::CreateVAB(VkFormat format,uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode)
{
    if(count==0)return(nullptr);

    const uint32_t stride=GetStrideByFormat(format);

    if(stride==0)
    {
        std::cerr<<"format["<<format<<"] stride length is 0,please use \"VulkanDevice::CreateBuffer(VkBufferUsageFlags,VkDeviceSize,VkSharingMode)\" function.";
        return(nullptr);
    }

    const VkDeviceSize size=stride*count;

    if(policy==BufferAllocPolicy::Auto)
    {
        if(attr->physical_device->HasReBAR())
            policy=BufferAllocPolicy::CPUVisible;
        else
            policy=BufferAllocPolicy::StagedUpload;
    }

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,size,data,sharing_mode);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=size;

        return(new VertexAttribBuffer(attr->device,buf,format,stride,count,staged));
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,size,size,data,sharing_mode,mem_usage))
        return(nullptr);

    return(new VertexAttribBuffer(attr->device,buf,format,stride,count));
}

const bool VulkanDevice::IsSupport(const IndexType &type)const
{
    if(type==IndexType::U16)return(true);
    if(type==IndexType::U8 &&attr->uint8_index_type)return(true);
    if(type==IndexType::U32&&attr->uint32_index_type)return(true);
    return(false);
}

const IndexType VulkanDevice::ChooseIndexType(const VkDeviceSize &vertex_count)const
{
    if(vertex_count<=0)return(IndexType::ERR);

    if(attr->uint8_index_type&& vertex_count<=0xFF  )return IndexType::U8;  else
    if(                         vertex_count<=0xFFFF)return IndexType::U16; else
    if(attr->uint32_index_type  )return IndexType::U32; else

    return IndexType::ERR;
}

const bool VulkanDevice::CheckIndexType(const IndexType it,const VkDeviceSize &vertex_count)const
{
    if(vertex_count<=0)return(false);

    if(it==IndexType::U16&&vertex_count<=0xFFFF)return(true);

    if(it==IndexType::U32&&                     attr->uint32_index_type)return(true);

    if(it==IndexType::U8 &&vertex_count<=0xFF&& attr->uint8_index_type)return(true);

    return(false);
}

IndexBuffer *VulkanDevice::CreateIBO(IndexType index_type,uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode)
{
    if(count==0)return(nullptr);

    uint32_t stride;

    if(index_type==IndexType::U8 )stride=1;else
    if(index_type==IndexType::U16)stride=2;else
    if(index_type==IndexType::U32)stride=4;else
        return(nullptr);

    const VkDeviceSize size=stride*count;

    if(policy==BufferAllocPolicy::Auto)
    {
        if(attr->physical_device->HasReBAR())
            policy=BufferAllocPolicy::CPUVisible;
        else
            policy=BufferAllocPolicy::StagedUpload;
    }

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,size,data,sharing_mode);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=size;

        return(new IndexBuffer(attr->device,buf,index_type,count,staged));
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,size,size,data,sharing_mode,mem_usage))
        return(nullptr);

    return(new IndexBuffer(attr->device,buf,index_type,count));
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode)
{
    return CreateBuffer(buf_usage,range,size,data,BufferAllocPolicy::Auto,sharing_mode);
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode)
{
    if(size<=0)return(nullptr);

    if(policy==BufferAllocPolicy::Auto)
    {
        if(attr->physical_device->HasReBAR())
            policy=BufferAllocPolicy::CPUVisible;
        else
            policy=BufferAllocPolicy::StagedUpload;
    }

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(buf_usage,size,data,sharing_mode);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=range;

        return(new DeviceBuffer(attr->device,buf,staged));
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;

    if(!CreateBuffer(&buf,buf_usage,range,size,data,sharing_mode,mem_usage))
        return(nullptr);

    return(new DeviceBuffer(attr->device,buf));
}
VK_NAMESPACE_END
