#ifndef HGL_GRAPH_VULKAN_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_BUFFER_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN

struct VulkanBuffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDescriptorBufferInfo info;
};//struct VulkanBuffer

class Buffer
{
protected:

    VkDevice device;
    VulkanBuffer buf;

private:

    friend class Device;
    friend class VertexBuffer;

    Buffer(VkDevice d,const VulkanBuffer &vb)
    {
        device=d;
        buf=vb;
    }

public:

    virtual ~Buffer();

    operator VkBuffer                   (){return buf.buffer;}
    operator VkDeviceMemory             (){return buf.memory;}
    operator VkDescriptorBufferInfo *   (){return &buf.info;}

    virtual uint8_t *Map(uint32_t start=0,uint32_t size=0);
    void Unmap();
};//class Buffer

class VertexBuffer:public Buffer
{
    VkFormat format;                    ///<数据格式
    uint32_t stride;                    ///<单个数据字节数
    uint32_t count;                     ///<数据数量

private:

    friend class Device;

    VertexBuffer(VkDevice d,const VulkanBuffer &vb,VkFormat fmt,uint32_t _stride,uint32_t _count):Buffer(d,vb)
    {
        format=fmt;
        stride=_stride;
        count=_count;
    }

public:

    ~VertexBuffer()=default;

    const VkFormat GetFormat()const { return format; }
    const uint32_t GetStride()const { return stride; }
    const uint32_t GetCount ()const { return count; }

    uint8_t *Map(uint32_t start=0,uint32_t size=0) override
    {
        return Buffer::Map(start*stride,size*stride);
    }
};//class VertexBuffer:public Buffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_BUFFER_INCLUDE
