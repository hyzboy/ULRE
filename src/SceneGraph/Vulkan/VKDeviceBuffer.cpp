#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKPhysicalDevice.h>

VK_NAMESPACE_BEGIN
const VkDeviceSize GPUDevice::GetUBOAlign()
{
    return attr->physical_device->GetUBOAlign();
}

bool GPUDevice::CreateBuffer(DeviceBufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode)
{
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

    DeviceMemory *dm=CreateMemory(mem_reqs,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

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

VBO *GPUDevice::CreateVBO(VkFormat format,uint32_t count,const void *data,SharingMode sharing_mode)
{
    const uint32_t stride=GetStrideByFormat(format);

    if(stride==0)
    {
        std::cerr<<"format["<<format<<"] stride length is 0,please use \"GPUDevice::CreateBuffer(VkBufferUsageFlags,VkDeviceSize,VkSharingMode)\" function.";
        return(nullptr);
    }

    const VkDeviceSize size=stride*count;

    DeviceBufferData buf;

    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,size,size,data,sharing_mode))
        return(nullptr);

    return(new VertexAttribBuffer(attr->device,buf,format,stride,count));
}

IndexBuffer *GPUDevice::CreateIBO(IndexType index_type,uint32_t count,const void *data,SharingMode sharing_mode)
{
    uint32_t stride;

    if(index_type==IndexType::U16)stride=2;else
    if(index_type==IndexType::U32)stride=4;else
        return(nullptr);

    const VkDeviceSize size=stride*count;

    DeviceBufferData buf;

    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,size,data,sharing_mode))
        return(nullptr);

    return(new IndexBuffer(attr->device,buf,index_type,count));
}

DeviceBuffer *GPUDevice::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize range,VkDeviceSize size,const void *data,SharingMode sharing_mode)
{
    DeviceBufferData buf;

    if(!CreateBuffer(&buf,buf_usage,range,size,data,sharing_mode))
        return(nullptr);

    return(new DeviceBuffer(attr->device,buf));
}
VK_NAMESPACE_END
