#include<hgl/vk/VKDevice.h>
#include<hgl/vk/buffer/BufferView.h>
#include<hgl/vk/buffer/StagedBuffer.h>
#include<hgl/vk/buffer/ReBarBuffer.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/log/Log.h>
#include<hgl/vk/buffer/VertexAttribBuffer.h>
#include"BufferPolicyResolve.h"

namespace hgl::graph{

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

    policy = ResolveBufferPolicy(this, policy);

    if(policy==BufferAllocPolicy::GPUDeviceLocal)
    {
        // 纯 device-local（无 host 影子）：内容由 GpuUploadQueue BufferToBuffer DMA 写入，禁止 Map/Write
        if(data)
            LogWarning("GPUDeviceLocal policy ignores inline data; contents must be uploaded via GpuUploadQueue DMA");

        DeviceBufferData buf;
        if(!CreateBuffer(&buf,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,size,size,nullptr,sharing_mode,MemoryUsage::GPUOnly,ObjectNameBuilder("VAB:Memory"),loc))
            return(nullptr);

        VertexAttribBuffer *vab = new VertexAttribBuffer(attr->device,buf,format,stride,count);
        vab->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
        TrackBuffer(vab, ObjectNameBuilder("VAB"), loc);
        return vab;
    }

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(ObjectNameBuilder("VAB"), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, size, data, sharing_mode, loc);
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
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,size,size,data,sharing_mode,mem_usage,ObjectNameBuilder("VAB:Memory"),loc))
        return(nullptr);

    // CPUVisible: install ReBarBuffer so GetGPUBuffer() always yields a valid IGPUBuffer*
    ReBarBuffer *rebar = new ReBarBuffer("VAB", attr->device, buf.buffer, buf.memory, size);
    VertexAttribBuffer *vab = new VertexAttribBuffer(attr->device,buf,format,stride,count);
    vab->SetStagedSource(rebar);
    vab->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
    TrackBuffer(vab, ObjectNameBuilder("VAB"), loc);
    return vab;
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

    policy = ResolveBufferPolicy(this, policy);

    if(policy==BufferAllocPolicy::GPUDeviceLocal)
    {
        // 纯 device-local（无 host 影子）：内容由 GpuUploadQueue BufferToBuffer DMA 写入，禁止 Map/Write
        if(data)
            LogWarning("GPUDeviceLocal policy ignores inline data; contents must be uploaded via GpuUploadQueue DMA");

        ObjectNameBuilder memory_name = name.base_name[0] == '\0'
            ? ObjectNameBuilder("Memory")
            : ObjectNameBuilder(AnsiString(name.base_name) + ".Memory");

        DeviceBufferData buf;
        if(!CreateBuffer(&buf,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,size,size,nullptr,sharing_mode,MemoryUsage::GPUOnly,memory_name,loc))
            return(nullptr);

        VertexAttribBuffer *vab = new VertexAttribBuffer(attr->device,buf,format,stride,count);
        vab->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
        TrackBuffer(vab, name, loc);
        return vab;
    }

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(name, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, size, data, sharing_mode, loc);
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

    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,size,size,data,sharing_mode,mem_usage,memory_name,loc))
        return(nullptr);

    // CPUVisible: install ReBarBuffer so GetGPUBuffer() always yields a valid IGPUBuffer*
    ReBarBuffer *rebar = new ReBarBuffer(
        name.base_name[0] ? std::string(name.base_name) : std::string("VAB"),
        attr->device, buf.buffer, buf.memory, size);
    VertexAttribBuffer *vab = new VertexAttribBuffer(attr->device,buf,format,stride,count);
    vab->SetStagedSource(rebar);
    vab->SetUpdateClass(update_class == BufferUpdateClass::Default ? BufferUpdateClass::MeshStatic : update_class);
    TrackBuffer(vab, name, loc);
    return vab;
}

}//namespace hgl::graph
