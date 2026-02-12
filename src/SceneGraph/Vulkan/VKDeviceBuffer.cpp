#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKBufferAccessBase.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/BufferPolicyImpl.h>
#include<hgl/log/Log.h>
#include<iostream>

VK_NAMESPACE_BEGIN
static BufferAllocPolicy ResolvePolicy(VulkanDevice *device, BufferAllocPolicy policy)
{
    if(policy!=BufferAllocPolicy::Auto)
        return policy;

    if(device->GetPhyDevice()->HasReBAR())
        return BufferAllocPolicy::CPUVisible;

    return BufferAllocPolicy::StagedUpload;
}

static BufferCommitPolicy SelectCommitPolicy(BufferUpdateClass update_class, VkBufferUsageFlags usage, BufferAllocPolicy policy)
{
    if(update_class == BufferUpdateClass::Manual)
        return BufferCommitPolicy::Manual;

    if(update_class == BufferUpdateClass::CriticalPerFrame || update_class == BufferUpdateClass::TransformData)
        return BufferCommitPolicy::Always;

    if(update_class == BufferUpdateClass::MeshStatic || update_class == BufferUpdateClass::MeshDynamic ||
       update_class == BufferUpdateClass::TextureTile || update_class == BufferUpdateClass::Particle ||
       update_class == BufferUpdateClass::Deferred)
        return BufferCommitPolicy::StagedOnly;

    if(policy == BufferAllocPolicy::StagedUpload || policy == BufferAllocPolicy::GPUOnly)
        return BufferCommitPolicy::StagedOnly;

    if((usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) || (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
        return BufferCommitPolicy::Always;

    return BufferCommitPolicy::Auto;
}

static void ApplyUpdateClass(DeviceBuffer *buf, BufferUpdateClass update_class, VkBufferUsageFlags usage, BufferAllocPolicy policy)
{
    if(!buf)
        return;

    buf->SetUpdateClass(update_class);
    buf->SetCommitPolicy(SelectCommitPolicy(update_class, usage, policy));
}

static void ApplyUpdateClassWithPolicy(VulkanDevice *device, DeviceBuffer *buf, BufferUpdateClass update_class,
                                       VkBufferUsageFlags usage, BufferAllocPolicy policy)
{
    ApplyUpdateClass(buf, update_class, usage, policy);

    if(!buf)
        return;

    const BufferPolicy *hardcoded = GetPolicyForUpdateClass(update_class);
    if(hardcoded)
        ApplyPolicy(buf, hardcoded);
}

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

VAB *VulkanDevice::CreateVAB(VkFormat format,uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class)
{
    if(count==0)return(nullptr);

    const uint32_t stride=GetStrideByFormat(format);

    if(stride==0)
    {
        std::cerr<<"format["<<format<<"] stride length is 0,please use \"VulkanDevice::CreateBuffer(VkBufferUsageFlags,VkDeviceSize,VkSharingMode)\" function.";
        return(nullptr);
    }

    const VkDeviceSize size=stride*count;

    policy = ResolvePolicy(this, policy);

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

            VertexAttribBuffer *vab = new VertexAttribBuffer(this,attr->device,buf,format,stride,count,staged);
            ApplyUpdateClassWithPolicy(this, vab, update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, policy);
            vab->SetAutoCommitProxy(new RawBufferAccessor(vab));
            return vab;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,size,size,data,sharing_mode,mem_usage))
        return(nullptr);

        VertexAttribBuffer *vab = new VertexAttribBuffer(this,attr->device,buf,format,stride,count);
        ApplyUpdateClassWithPolicy(this, vab, update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, policy);
        vab->SetAutoCommitProxy(new RawBufferAccessor(vab));
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

IndexBuffer *VulkanDevice::CreateIBO(IndexType index_type,uint32_t count,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class)
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
        StagedBuffer *staged=CreateStagedBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,size,data,sharing_mode);
        if(!staged)
            return(nullptr);

        DeviceBufferData buf;
        buf.buffer=staged->GetDeviceBuffer();
        buf.memory=staged->GetDeviceMemory();
        buf.info.buffer=buf.buffer;
        buf.info.offset=0;
        buf.info.range=size;

            IndexBuffer *ibo = new IndexBuffer(this,attr->device,buf,index_type,count,staged);
            ApplyUpdateClassWithPolicy(this, ibo, update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class,
                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, policy);
            ibo->SetAutoCommitProxy(new RawBufferAccessor(ibo));
            return ibo;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,size,size,data,sharing_mode,mem_usage))
        return(nullptr);

        IndexBuffer *ibo = new IndexBuffer(this,attr->device,buf,index_type,count);
        ApplyUpdateClassWithPolicy(this, ibo, update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class,
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, policy);
        ibo->SetAutoCommitProxy(new RawBufferAccessor(ibo));
        return ibo;
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode)
{
    return CreateBuffer(buf_usage,range,size,data,BufferAllocPolicy::Auto,sharing_mode);
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode)
{
    if(size<=0)return(nullptr);

    policy = ResolvePolicy(this, policy);

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

            DeviceBuffer *dev_buf = new DeviceBuffer(this,attr->device,buf,staged);
            ApplyUpdateClassWithPolicy(this, dev_buf, BufferUpdateClass::Default, buf_usage, policy);
        dev_buf->SetAutoCommitProxy(new RawBufferAccessor(dev_buf));
        return dev_buf;
    }

    MemoryUsage mem_usage=MemoryUsage::CPUOnly;
    if(policy==BufferAllocPolicy::CPUVisible)
        mem_usage=MemoryUsage::ReBAR;
    else if(policy==BufferAllocPolicy::Readback)
        mem_usage=MemoryUsage::GPUToCPU;

    DeviceBufferData buf;

    if(!CreateBuffer(&buf,buf_usage,range,size,data,sharing_mode,mem_usage))
        return(nullptr);

        DeviceBuffer *dev_buf = new DeviceBuffer(this,attr->device,buf);
        ApplyUpdateClassWithPolicy(this, dev_buf, BufferUpdateClass::Default, buf_usage, policy);
    dev_buf->SetAutoCommitProxy(new RawBufferAccessor(dev_buf));
    return dev_buf;
}

    DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode,BufferUpdateClass update_class)
    {
        DeviceBuffer *buf = CreateBuffer(buf_usage,range,size,data,policy,sharing_mode);
        ApplyUpdateClassWithPolicy(this, buf, update_class, buf_usage, ResolvePolicy(this, policy));
        return buf;
    }

    DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode,BufferUpdateClass update_class)
    {
        return CreateBuffer(buf_usage,range,size,data,BufferAllocPolicy::Auto,sharing_mode,update_class);
    }
VK_NAMESPACE_END
