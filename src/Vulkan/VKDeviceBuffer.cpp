#include<hgl/vk/VKDevice.h>
#include<hgl/common/SSBOOffsetHelper.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKBufferAccessBase.h>
#include<hgl/vk/VKStagedBuffer.h>
#include<hgl/vk/VKReBarBuffer.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/log/Log.h>
#include<iostream>
#include<cstring>

namespace hgl::graph{

static BufferAllocPolicy ResolvePolicy(VulkanDevice *, BufferAllocPolicy policy)
{
    return policy;
}

static VmaMemoryUsage ToVmaMemoryUsage(const MemoryUsage mem_usage)
{
    switch(mem_usage)
    {
    case MemoryUsage::GPUOnly:    return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    case MemoryUsage::ReBAR:      return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    default:                      return VMA_MEMORY_USAGE_AUTO;
    }
}

static VmaAllocationCreateFlags ToVmaAllocationFlags(const MemoryUsage mem_usage)
{
    switch(mem_usage)
    {
    case MemoryUsage::GPUOnly:
        return 0;

    case MemoryUsage::GPUToCPU:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

    case MemoryUsage::CPUOnly:
    case MemoryUsage::CPUToGPU:
    case MemoryUsage::ReBAR:
    default:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }
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

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = ToVmaMemoryUsage(mem_usage);
    alloc_info.flags = ToVmaAllocationFlags(mem_usage);

    if(vmaCreateBuffer(vma_allocator,
                       static_cast<const VkBufferCreateInfo *>(&buf_info),
                       &alloc_info,
                       &buf->buffer,
                       &buf->allocation,
                       nullptr)!=VK_SUCCESS)
    {
        return(false);
    }

    VmaAllocationInfo allocation_info{};
    vmaGetAllocationInfo(vma_allocator, buf->allocation, &allocation_info);
    buf->vk_memory = allocation_info.deviceMemory;

    TrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)buf->buffer, name, loc);
    if(buf->vk_memory)
        TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)buf->vk_memory, name.Append(ObjectTypeTag::VKMemory), loc);

    buf->info.buffer  =buf->buffer;
    buf->info.offset  =0;
    buf->info.range   =range;

    if(!data)
        return(true);

    void *mapped=nullptr;
    if(vmaMapMemory(vma_allocator,buf->allocation,&mapped)!=VK_SUCCESS||!mapped)
    {
        vmaDestroyBuffer(vma_allocator, buf->buffer, buf->allocation);
        buf->buffer = VK_NULL_HANDLE;
        buf->allocation = VK_NULL_HANDLE;
        buf->vk_memory = VK_NULL_HANDLE;
        return(false);
    }

    std::memcpy(mapped,data,static_cast<size_t>(size));
    vmaUnmapMemory(vma_allocator,buf->allocation);
    return(true);
}

VAB *VulkanDevice::CreateVAB(VkFormat format,uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class, const std::source_location &loc,bool prefer_storage_usage)
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

    const VkBufferUsageFlags vab_usage = static_cast<VkBufferUsageFlags>(ComputeVABUsageFlags(prefer_storage_usage));

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(ObjectNameBuilder("VAB"), vab_usage, size, data, sharing_mode, loc);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetVkDeviceBuffer();
        buf.allocation=VK_NULL_HANDLE;
        buf.vk_memory=staged->GetVkDeviceMemory();
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
    if(!CreateBuffer(&buf,vab_usage,size,size,data,sharing_mode,mem_usage,ObjectNameBuilder("VAB:Memory"),loc))
        return(nullptr);

    // CPUVisible: install ReBarBuffer so GetGPUBuffer() always yields a valid IGPUBuffer*
    ReBarBuffer *rebar = new ReBarBuffer("VAB", vma_allocator, buf.buffer, buf.allocation, size);
    VertexAttribBuffer *vab = new VertexAttribBuffer(attr->device,buf,format,stride,count);
    vab->SetStagedSource(rebar);
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

IndexBuffer *VulkanDevice::CreateIBO(const ObjectNameBuilder &name, IndexType index_type, uint32_t count, const void *data, BufferAllocPolicy policy, SharingMode sharing_mode, BufferUpdateClass update_class, const std::source_location &loc)
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
        StagedBuffer *staged=CreateStagedBuffer(name, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, size, data, sharing_mode, loc);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetVkDeviceBuffer();
        buf.allocation=VK_NULL_HANDLE;
        buf.vk_memory=staged->GetVkDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=size;

        IndexBuffer *ibo = new IndexBuffer(attr->device,buf,index_type,count);
        ibo->SetStagedSource(staged);
        ibo->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
        TrackBuffer(ibo, name, loc);
        return ibo;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    ObjectNameBuilder memory_name = name.base_name[0] == '\0'
        ? ObjectNameBuilder("Memory")
        : ObjectNameBuilder(AnsiString(name.base_name) + ".Memory");

    DeviceBufferData buf;
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,size,size,data,sharing_mode,mem_usage,memory_name,loc))
        return(nullptr);

    // CPUVisible: install ReBarBuffer so GetGPUBuffer() always yields a valid IGPUBuffer*
    ReBarBuffer *rebar = new ReBarBuffer(
        name.base_name[0] ? std::string(name.base_name) : std::string("IBO"),
        vma_allocator, buf.buffer, buf.allocation, size);
    IndexBuffer *ibo = new IndexBuffer(attr->device,buf,index_type,count);
    ibo->SetStagedSource(rebar);
    ibo->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
    TrackBuffer(ibo, name, loc);
    return ibo;
}

IndexBuffer *VulkanDevice::CreateIBO(IndexType index_type,uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class, const std::source_location &loc)
{
    return CreateIBO(ObjectNameBuilder("IBO"), index_type, count, data, policy, sharing_mode, update_class, loc);
}

VkBufferOwner *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode, const std::source_location &loc)
{
    return CreateBuffer(buf_usage,range,size,data,BufferAllocPolicy::Auto,sharing_mode,loc);
}

VkBufferOwner *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode, const std::source_location &loc)
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
        buf.allocation=VK_NULL_HANDLE;
        buf.vk_memory=staged->GetVkDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=range;

        VkBufferOwner *dev_buf = new VkBufferOwner(attr->device,buf);
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

    // CPUVisible: install ReBarBuffer so GetGPUBuffer() always yields a valid IGPUBuffer*
    ReBarBuffer *rebar = new ReBarBuffer(std::string(buffer_type), vma_allocator, buf.buffer, buf.allocation, size);
    VkBufferOwner *dev_buf = new VkBufferOwner(attr->device,buf);
    dev_buf->SetStagedSource(rebar);
    TrackBuffer(dev_buf, ObjectNameBuilder(buffer_type), loc);
    return dev_buf;
}

VkBufferOwner *VulkanDevice::CreateBuffer(const ObjectNameBuilder &name,
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
        buf.allocation=VK_NULL_HANDLE;
        buf.vk_memory=staged->GetVkDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=range;

        VkBufferOwner *dev_buf = new VkBufferOwner(attr->device,buf);
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

    // CPUVisible: install ReBarBuffer so GetGPUBuffer() always yields a valid IGPUBuffer*
    ReBarBuffer *rebar = new ReBarBuffer(
        name.base_name[0] ? std::string(name.base_name) : std::string("Buffer"),
        vma_allocator, buf.buffer, buf.allocation, size);
    VkBufferOwner *dev_buf = new VkBufferOwner(attr->device,buf);
    dev_buf->SetStagedSource(rebar);
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
                             const std::source_location &loc,
                             bool prefer_storage_usage)
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

    const VkBufferUsageFlags vab_usage = static_cast<VkBufferUsageFlags>(ComputeVABUsageFlags(prefer_storage_usage));

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(name, vab_usage, size, data, sharing_mode, loc);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetVkDeviceBuffer();
        buf.allocation=VK_NULL_HANDLE;
        buf.vk_memory=staged->GetVkDeviceMemory();
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

    if(!CreateBuffer(&buf,vab_usage,size,size,data,sharing_mode,mem_usage,memory_name,loc))
        return(nullptr);

    // CPUVisible: install ReBarBuffer so GetGPUBuffer() always yields a valid IGPUBuffer*
    ReBarBuffer *rebar = new ReBarBuffer(
        name.base_name[0] ? std::string(name.base_name) : std::string("VAB"),
        vma_allocator, buf.buffer, buf.allocation, size);
    VertexAttribBuffer *vab = new VertexAttribBuffer(attr->device,buf,format,stride,count);
    vab->SetStagedSource(rebar);
    vab->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
    TrackBuffer(vab, name, loc);
    return vab;
}

VkBufferOwner *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class, const std::source_location &loc)
{
    VkBufferOwner *buf = CreateBuffer(buf_usage,range,size,data,policy,sharing_mode,loc);
    if(buf) buf->SetUpdateClass(update_class);
    return buf;
}

VkBufferOwner *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,BufferUpdateClass update_class, const std::source_location &loc)
{
    return CreateBuffer(buf_usage,range,size,data,BufferAllocPolicy::Auto,sharing_mode,update_class,loc);
}
}//namespace hgl::graph
