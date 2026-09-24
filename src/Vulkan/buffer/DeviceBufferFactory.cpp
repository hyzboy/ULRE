#include<hgl/vk/VKDevice.h>
#include<hgl/vk/buffer/BufferView.h>
#include<hgl/vk/buffer/StagedBuffer.h>
#include<hgl/vk/buffer/ReBarBuffer.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/log/Log.h>
#include"BufferPolicyResolve.h"

namespace hgl::graph{

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

    // BDA usage 的内存必须带 DEVICE_ADDRESS 分配 flag（vkGetBufferDeviceAddress 前置条件）
    const VkMemoryAllocateFlags alloc_flags=
        (buf_usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            ?VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT:0;

    DeviceMemory *dm=CreateMemory(mem_reqs,mem_usage,name,loc,alloc_flags);

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
DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode, const std::source_location &loc)
{
    return CreateBuffer(buf_usage,range,size,data,BufferAllocPolicy::Auto,sharing_mode,loc);
}

DeviceBuffer *VulkanDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,BufferAllocPolicy policy,SharingMode sharing_mode, const std::source_location &loc)
{
    if(size<=0)return(nullptr);

    if((buf_usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) && !(buf_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT))
        policy = BufferAllocPolicy::CPUVisible;
    else
        policy = ResolveBufferPolicy(this, policy);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        // Generate meaningful name based on buffer usage
        const char* buffer_type = "Buffer";
        if(buf_usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) buffer_type = "UniformBuffer";
        else if(buf_usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) buffer_type = "StorageBuffer";
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
    else if(buf_usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) buffer_type = "IndexBuffer";
    else if(buf_usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) buffer_type = "IndirectBuffer";
    else if(buf_usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) buffer_type = "TransferSrcBuffer";
    else if(buf_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) buffer_type = "TransferDstBuffer";

    AnsiString memory_name = AnsiString(buffer_type) + ":Memory";

    if(!CreateBuffer(&buf,buf_usage,range,size,data,sharing_mode,mem_usage,ObjectNameBuilder(memory_name.c_str()),loc))
        return(nullptr);

    // CPUVisible: install ReBarBuffer so GetGPUBuffer() always yields a valid IGPUBuffer*
    ReBarBuffer *rebar = new ReBarBuffer(std::string(buffer_type), attr->device, buf.buffer, buf.memory, size);
    DeviceBuffer *dev_buf = new DeviceBuffer(attr->device,buf);
    dev_buf->SetStagedSource(rebar);
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

    if((buf_usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) && !(buf_usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT))
        policy = BufferAllocPolicy::CPUVisible;
    else
        policy = ResolveBufferPolicy(this, policy);

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

    // CPUVisible: install ReBarBuffer so GetGPUBuffer() always yields a valid IGPUBuffer*
    ReBarBuffer *rebar = new ReBarBuffer(
        name.base_name[0] ? std::string(name.base_name) : std::string("Buffer"),
        attr->device, buf.buffer, buf.memory, size);
    DeviceBuffer *dev_buf = new DeviceBuffer(attr->device,buf);
    dev_buf->SetStagedSource(rebar);
    dev_buf->SetUpdateClass(update_class);
    TrackBuffer(dev_buf, name, loc);
    return dev_buf;
}

DeviceBuffer *VulkanDevice::CreateArenaBuffer(const AnsiString &name,VkDeviceSize size,SharingMode sharing_mode, const std::source_location &loc)
{
    if(size<=0)return(nullptr);

    // 材质数据 Arena：整块 HOST_VISIBLE 直写 + shader 侧设备地址寻址。
    // 自包含实现而非复用 CreateBuffer(...policy...)——后者不携带
    // VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT 分配 flag，且 Auto 策略在无
    // ReBAR 设备会落到 StagedUpload（无整块可持久映射的 CPU 可见内存）。
    const VkBufferUsageFlags buf_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                       | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    BufferCreateInfo buf_info;

    buf_info.usage                  = buf_usage;
    buf_info.size                   = size;
    buf_info.queueFamilyIndexCount  = 0;
    buf_info.pQueueFamilyIndices    = nullptr;
    buf_info.sharingMode            = VkSharingMode(sharing_mode);

    DeviceBufferData buf;
    const ObjectNameBuilder obj_name(name.c_str());

    if(vkCreateBuffer(attr->device,&buf_info,nullptr,&buf.buffer)!=VK_SUCCESS)
        return(nullptr);

    TrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)buf.buffer, obj_name, loc);

    VkMemoryRequirements mem_reqs;

    vkGetBufferMemoryRequirements(attr->device,buf.buffer,&mem_reqs);

    // 内存类型：ReBAR 设备优先 DEVICE_LOCAL 的可映射显存；否则退纯 HOST_VISIBLE|HOST_COHERENT
    uint32_t properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                        | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                        | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if(attr->physical_device->GetMemoryType(mem_reqs.memoryTypeBits,properties)<0)
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    ObjectNameBuilder memory_name(AnsiString(name) + ".Memory");

    DeviceMemory *dm=CreateMemory(mem_reqs,properties,VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,memory_name,loc);

    if(dm&&dm->BindBuffer(buf.buffer))
    {
        buf.info.buffer =buf.buffer;
        buf.info.offset =0;
        buf.info.range  =size;

        buf.memory      =dm;

        ReBarBuffer *rebar = new ReBarBuffer(std::string(name.c_str()), attr->device, buf.buffer, dm, size);

        DeviceBuffer *dev_buf = new DeviceBuffer(attr->device,buf);
        dev_buf->SetStagedSource(rebar);
        TrackBuffer(dev_buf, obj_name, loc);
        return dev_buf;
    }

    delete dm;
    vkDestroyBuffer(attr->device,buf.buffer,nullptr);
    return(nullptr);
}

uint64_t VulkanDevice::GetBufferDeviceAddress(VkBuffer buf) const
{
    if(buf==VK_NULL_HANDLE)
        return(0);

    VkBufferDeviceAddressInfo addr_info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addr_info.buffer    = buf;

    return vkGetBufferDeviceAddress(attr->device,&addr_info);
}

uint64_t VulkanDevice::GetBufferDeviceAddressAligned16(VkBuffer buf) const
{
    const uint64_t addr = GetBufferDeviceAddress(buf);

    // BDA 编码规范：buffer_reference_align=16 的承诺必须由基址兑现。
    // 承诺不成立 = 未定义行为（Validation Layer 不查——fail-fast 并记错误日志）
    if(addr && (addr & 0xFull)!=0ull)
    {
        GLogError("[BDA] buffer device address 0x%llx not 16-byte aligned -- "
                  "buffer_reference_align=16 promise broken",
                  (unsigned long long)addr);
        return 0;
    }

    return addr;
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
