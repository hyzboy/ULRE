#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKBuffer.h>

VK_NAMESPACE_BEGIN
bool Device::CreateBuffer(BufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize size,const void *data,VkSharingMode sharing_mode)
{
    VkBufferCreateInfo buf_info={};
    buf_info.sType                  =VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.pNext                  =nullptr;
    buf_info.usage                  =buf_usage;
    buf_info.size                   =size;
    buf_info.queueFamilyIndexCount  =0;
    buf_info.pQueueFamilyIndices    =nullptr;
    buf_info.sharingMode            =sharing_mode;
    buf_info.flags                  =0;

    if(vkCreateBuffer(attr->device,&buf_info,nullptr,&buf->buffer)!=VK_SUCCESS)
        return(false);

    VkMemoryRequirements mem_reqs;

    vkGetBufferMemoryRequirements(attr->device,buf->buffer,&mem_reqs);

    Memory *dm=CreateMemory(mem_reqs,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if(dm&&dm->BindBuffer(buf->buffer))
    {
        buf->info.buffer  =buf->buffer;
        buf->info.offset  =0;
        buf->info.range   =size;

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

VertexBuffer *Device::CreateVBO(VkFormat format,uint32_t count,const void *data,VkSharingMode sharing_mode)
{
    const uint32_t stride=GetStrideByFormat(format);

    if(stride==0)
    {
        std::cerr<<"format["<<format<<"] stride length is 0,please use \"Device::CreateBuffer(VkBufferUsageFlags,VkDeviceSize,VkSharingMode)\" function.";
        return(nullptr);
    }

    const VkDeviceSize size=stride*count;

    BufferData buf;

    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,size,data,sharing_mode))
        return(nullptr);

    return(new VertexBuffer(attr->device,buf,format,stride,count));
}

IndexBuffer *Device::CreateIBO(VkIndexType index_type,uint32_t count,const void *data,VkSharingMode sharing_mode)
{
    uint32_t stride;

    if(index_type==VK_INDEX_TYPE_UINT16)stride=2;else
    if(index_type==VK_INDEX_TYPE_UINT32)stride=4;else
        return(nullptr);

    const VkDeviceSize size=stride*count;

    BufferData buf;

    if(!CreateBuffer(&buf,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,size,data,sharing_mode))
        return(nullptr);

    return(new IndexBuffer(attr->device,buf,index_type,count));
}

Buffer *Device::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,const void *data,VkSharingMode sharing_mode)
{
    BufferData buf;

    if(!CreateBuffer(&buf,buf_usage,size,data,sharing_mode))
        return(nullptr);

    return(new Buffer(attr->device,buf));
}
VK_NAMESPACE_END
