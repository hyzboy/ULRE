#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKBufferAccessBase.h>
#include<hgl/vk/VKStagedBuffer.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/log/Log.h>
#include<iostream>

namespace hgl::graph{

static BufferAllocPolicy ResolvePolicy(VulkanDevice *device, BufferAllocPolicy policy)
{
    if(policy!=BufferAllocPolicy::Auto)
        return policy;

    if(device->GetPhyDevice()->HasReBAR())
        return BufferAllocPolicy::CPUVisible;

    return BufferAllocPolicy::StagedUpload;
}

const VkDeviceSize VulkanDevice::GetUBOAlign   (){return attr->physical_device->GetUBOAlign();}
const VkDeviceSize VulkanDevice::GetSSBOAlign  (){return attr->physical_device->GetSSBOAlign();}
const VkDeviceSize VulkanDevice::GetUBORange   (){return attr->physical_device->GetUBORange();}
const VkDeviceSize VulkanDevice::GetSSBORange  (){return attr->physical_device->GetSSBORange();}

bool VulkanDevice::CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,const ObjectNameBuilder &name, const std::source_location &loc)
{
    return CreateBuffer(buf,buf_usage,range,size,data,sharing_mode,MemoryUsage::CPUOnly,name,loc);
}

bool VulkanDevice::CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,MemoryUsage mem_usage,const ObjectNameBuilder &name, const std::source_location &loc)
{
    assert(name.base_name[0] != '\0' && "ERROR: CreateBuffer called with empty name! Check the call stack to find where.");
    if(size<=0)return(false);

    BufferCreateInfo buf_info;

    buf_info.usage                  = buf_usage;
    buf_info.size                   = size;
    buf_info.queueFamilyIndexCount  = 0;
    buf_info.pQueueFamilyIndices    = nullptr;
    buf_info.sharingMode            = VkSharingMode(sharing_mode);

    if(vkCreateBuffer(attr->device,&buf_info,nullptr,&buf->buffer)!=VK_SUCCESS)
        return(false);

    TrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)buf->buffer, name, loc);

    VkMemoryRequirements mem_reqs;

    vkGetBufferMemoryRequirements(attr->device,buf->buffer,&mem_reqs);

#ifdef _DEBUG
    if(buf_usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
    {
        GLogWarning("[CreateBuffer] UBO size=%llu range=%llu memReqSize=%llu memReqAlign=%llu uboAlign=%llu uboRange=%llu",
                    static_cast<unsigned long long>(size),
                    static_cast<unsigned long long>(range),
                    static_cast<unsigned long long>(mem_reqs.size),
                    static_cast<unsigned long long>(mem_reqs.alignment),
                    static_cast<unsigned long long>(GetUBOAlign()),
                    static_cast<unsigned long long>(GetUBORange()));
    }
    if(buf_usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
    {
        GLogWarning("[CreateBuffer] SSBO size=%llu range=%llu memReqSize=%llu memReqAlign=%llu ssboAlign=%llu ssboRange=%llu",
                    static_cast<unsigned long long>(size),
                    static_cast<unsigned long long>(range),
                    static_cast<unsigned long long>(mem_reqs.size),
                    static_cast<unsigned long long>(mem_reqs.alignment),
                    static_cast<unsigned long long>(GetSSBOAlign()),
                    static_cast<unsigned long long>(GetSSBORange()));
    }
#endif//_DEBUG

    DeviceMemory *dm=CreateMemory(mem_reqs,mem_usage,name,loc);

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

VAB *VulkanDevice::CreateVAB(VkFormat format,uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class, const std::source_location &loc)
{
    if(count==0)return(nullptr);

    const uint32_t stride=GetStrideByFormat(format);

    if(stride==0)
    {
        LogError("format[",format,u"] stride length is 0, please use CreateBuffer(VkBufferUsageFlags,VkDeviceSize,VkSharingMode) function");
        return(nullptr);
    }

    const VkDeviceSize size=stride*count;

    policy = ResolvePolicy(this, policy);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(ObjectNameBuilder("VAB"), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, size, data, sharing_mode, loc);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetVkDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=size;

        VertexAttribBuffer *vab = new VertexAttribBuffer(attr->device,buf,format,stride,count);
        vab->SetStagedSource(staged);
        vab->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
        TrackBuffer(vab, ObjectNameBuilder("VAB"), loc);
        return vab;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,size,size,data,sharing_mode,mem_usage,ObjectNameBuilder("VAB:Memory"),loc))
        return(nullptr);

    VertexAttribBuffer *vab = new VertexAttribBuffer(attr->device,buf,format,stride,count);
    vab->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
    TrackBuffer(vab, ObjectNameBuilder("VAB"), loc);
    return vab;
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

IndexBuffer *VulkanDevice::CreateIBO(IndexType index_type,uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class, const std::source_location &loc)
{
    if(count==0)return(nullptr);

    uint32_t stride;

    if(index_type==IndexType::U8 )stride=1;else
    if(index_type==IndexType::U16)stride=2;else
    if(index_type==IndexType::U32)stride=4;else
        return(nullptr);

    const VkDeviceSize size=stride*count;

    policy = ResolvePolicy(this, policy);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(ObjectNameBuilder("IBO"), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, size, data, sharing_mode, loc);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetVkDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=size;

        IndexBuffer *ibo = new IndexBuffer(attr->device,buf,index_type,count);
        ibo->SetStagedSource(staged);
        ibo->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
        TrackBuffer(ibo, ObjectNameBuilder("IBO"), loc);
        return ibo;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,size,size,data,sharing_mode,mem_usage,ObjectNameBuilder("IBO:Memory"),loc))
        return(nullptr);

    IndexBuffer *ibo = new IndexBuffer(attr->device,buf,index_type,count);
    ibo->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
    TrackBuffer(ibo, ObjectNameBuilder("IBO"), loc);
    return ibo;
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode, const std::source_location &loc)
{
    return CreateBuffer(buf_usage,range,size,data,BufferAllocPolicy::Auto,sharing_mode,loc);
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode, const std::source_location &loc)
{
    if(size<=0)return(nullptr);

    policy = ResolvePolicy(this, policy);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        // Generate meaningful name based on buffer usage
        const char* buffer_type = "Buffer";
        if(buf_usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) buffer_type = "UniformBuffer";
        else if(buf_usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) buffer_type = "StorageBuffer";
        else if(buf_usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) buffer_type = "VertexBuffer";
        else if(buf_usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) buffer_type = "IndexBuffer";
        else if(buf_usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) buffer_type = "IndirectBuffer";
        else if(buf_usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) buffer_type = "TransferSrcBuffer";
        else if(buf_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) buffer_type = "TransferDstBuffer";

        StagedBuffer *staged=CreateStagedBuffer(ObjectNameBuilder(buffer_type), buf_usage, size, data, sharing_mode, loc);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetVkDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=range;

        DeviceBuffer *dev_buf = new DeviceBuffer(attr->device,buf);
        dev_buf->SetStagedSource(staged);
        TrackBuffer(dev_buf, ObjectNameBuilder(buffer_type), loc);
        return dev_buf;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;

    // Generate meaningful name based on buffer usage
    const char* buffer_type = "Buffer";
    if(buf_usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) buffer_type = "UniformBuffer";
    else if(buf_usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) buffer_type = "StorageBuffer";
    else if(buf_usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) buffer_type = "VertexBuffer";
    else if(buf_usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) buffer_type = "IndexBuffer";
    else if(buf_usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) buffer_type = "IndirectBuffer";
    else if(buf_usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) buffer_type = "TransferSrcBuffer";
    else if(buf_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) buffer_type = "TransferDstBuffer";

    AnsiString memory_name = AnsiString(buffer_type) + ":Memory";

    if(!CreateBuffer(&buf,buf_usage,range,size,data,sharing_mode,mem_usage,ObjectNameBuilder(memory_name.c_str()),loc))
        return(nullptr);

    DeviceBuffer *dev_buf = new DeviceBuffer(attr->device,buf);
    TrackBuffer(dev_buf, ObjectNameBuilder(buffer_type), loc);
    return dev_buf;
}

DeviceBuffer *VulkanDevice::CreateBuffer(const ObjectNameBuilder &name,
                                         VkBufferUsageFlags buf_usage,
                                         VkDeviceSize range,
                                         VkDeviceSize size,
                                         const void *data,
                                         BufferAllocPolicy policy,
                                         SharingMode sharing_mode,
                                         BufferUpdateClass update_class,
                                         const std::source_location &loc)
{
    if(size<=0)return(nullptr);

    policy = ResolvePolicy(this, policy);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(name, buf_usage, size, data, sharing_mode, loc);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetVkDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=range;

        DeviceBuffer *dev_buf = new DeviceBuffer(attr->device,buf);
        dev_buf->SetStagedSource(staged);
        dev_buf->SetUpdateClass(update_class);
        TrackBuffer(dev_buf, name, loc);
        return dev_buf;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;
    ObjectNameBuilder memory_name = name.base_name[0] == '\0'
        ? ObjectNameBuilder("Memory")
        : ObjectNameBuilder(AnsiString(name.base_name) + ".Memory");

    if(!CreateBuffer(&buf,buf_usage,range,size,data,sharing_mode,mem_usage,memory_name,loc))
        return(nullptr);

    DeviceBuffer *dev_buf = new DeviceBuffer(attr->device,buf);
    dev_buf->SetUpdateClass(update_class);
    TrackBuffer(dev_buf, name, loc);
    return dev_buf;
}

VAB *VulkanDevice::CreateVAB(const ObjectNameBuilder &name,
                             VkFormat format,
                             uint32_t count,
                             const void *data,
                             BufferAllocPolicy policy,
                             SharingMode sharing_mode,
                             BufferUpdateClass update_class,
                             const std::source_location &loc)
{
    if(count==0)return(nullptr);

    const uint32_t stride=GetStrideByFormat(format);

    if(stride==0)
    {
        LogError("format[",format,u"] stride length is 0, please use CreateBuffer(VkBufferUsageFlags,VkDeviceSize,VkSharingMode) function");
        return(nullptr);
    }

    const VkDeviceSize size=stride*count;

    policy = ResolvePolicy(this, policy);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(name, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, size, data, sharing_mode, loc);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetVkDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=size;

        VertexAttribBuffer *vab = new VertexAttribBuffer(attr->device,buf,format,stride,count);
        vab->SetStagedSource(staged);
        vab->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
        TrackBuffer(vab, name, loc);
        return vab;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;
    ObjectNameBuilder memory_name = name.base_name[0] == '\0'
        ? ObjectNameBuilder("Memory")
        : ObjectNameBuilder(AnsiString(name.base_name) + ".Memory");

    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,size,size,data,sharing_mode,mem_usage,memory_name,loc))
        return(nullptr);

    VertexAttribBuffer *vab = new VertexAttribBuffer(attr->device,buf,format,stride,count);
    vab->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
    TrackBuffer(vab, name, loc);
    return vab;
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class, const std::source_location &loc)
{
    DeviceBuffer *buf = CreateBuffer(buf_usage,range,size,data,policy,sharing_mode,loc);
    if(buf) buf->SetUpdateClass(update_class);
    return buf;
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,BufferUpdateClass update_class, const std::source_location &loc)
{
    return CreateBuffer(buf_usage,range,size,data,BufferAllocPolicy::Auto,sharing_mode,update_class,loc);
}
}//namespace hgl::graph