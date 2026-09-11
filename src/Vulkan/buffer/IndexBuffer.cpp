#include<hgl/vk/VKDevice.h>
#include<hgl/vk/buffer/BufferView.h>
#include<hgl/vk/buffer/StagedBuffer.h>
#include<hgl/vk/buffer/ReBarBuffer.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/log/Log.h>
#include<hgl/vk/buffer/IndexBuffer.h>
#include"BufferPolicyResolve.h"

namespace hgl::graph{

const bool VulkanDevice::IsSupport(const IndexType &type)const
{
    // 引擎统一 uint32 索引（废弃 U8/U16）
    return(type==IndexType::U32);
}

const IndexType VulkanDevice::ChooseIndexType(const VkDeviceSize &vertex_count)const
{
    // 引擎统一 uint32 索引——恒 U32（废弃 U8/U16 自动选型）
    if(vertex_count<=0)return(IndexType::ERR);

    return IndexType::U32;
}

const bool VulkanDevice::CheckIndexType(const IndexType it,const VkDeviceSize &vertex_count)const
{
    if(vertex_count<=0)return(false);

    // U8/U16 已废弃——引擎统一 uint32 索引
    if(it==IndexType::U32&&                     attr->uint32_index_type)return(true);

    return(false);
}

IndexBuffer *VulkanDevice::CreateIBO(const ObjectNameBuilder &name, IndexType index_type, uint32_t count, const void *data, BufferAllocPolicy policy, SharingMode sharing_mode, BufferUpdateClass update_class, const std::source_location &loc)
{
    if(count==0)return(nullptr);

    // 引擎统一 uint32 索引（U8/U16 已废弃）
    const uint32_t stride=4;

    const VkDeviceSize size=stride*count;

    policy = ResolveBufferPolicy(this, policy);

    if(policy==BufferAllocPolicy::StagedUpload||policy==BufferAllocPolicy::GPUOnly)
    {
        StagedBuffer *staged=CreateStagedBuffer(name, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, size, data, sharing_mode, loc);
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
    // 索引 buffer 同时作顶点索引 SSBO（SSBO 顶点输入——非索引绘制查表）
    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,size,size,data,sharing_mode,mem_usage,memory_name,loc))
        return(nullptr);

    // CPUVisible: install ReBarBuffer so GetGPUBuffer() always yields a valid IGPUBuffer*
    ReBarBuffer *rebar = new ReBarBuffer(
        name.base_name[0] ? std::string(name.base_name) : std::string("IBO"),
        attr->device, buf.buffer, buf.memory, size);
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

}//namespace hgl::graph
