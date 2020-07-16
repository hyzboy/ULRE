#ifndef HGL_GRAPH_VULKAN_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_BUFFER_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKMemory.h>
VK_NAMESPACE_BEGIN
struct BufferData
{
    VkBuffer                buffer;
    Memory *                memory=nullptr;
    VkDescriptorBufferInfo  info;
};//struct BufferData

class Buffer
{
protected:

    VkDevice device;
    BufferData buf;

private:

    friend class Device;
    friend class VertexAttribBuffer;
    friend class IndexBuffer;

    Buffer(VkDevice d,const BufferData &b)
    {
        device=d;
        buf=b;
    }

public:

    virtual ~Buffer();

            VkBuffer                   GetBuffer    (){return buf.buffer;}
            Memory *                   GetMemory    (){return buf.memory;}
    const   VkDescriptorBufferInfo *   GetBufferInfo()const{return &buf.info;}

            void *  Map()                                       {return buf.memory->Map();}
    virtual void *  Map(VkDeviceSize start,VkDeviceSize size)   {return buf.memory->Map(start,size);}
            void    Unmap()                                     {return buf.memory->Unmap();}

    bool    Write(const void *ptr,uint32_t start,uint32_t size) {return buf.memory->Write(ptr,start,size);}
    bool    Write(const void *ptr)                              {return buf.memory->Write(ptr);}
};//class Buffer

class VertexAttribBuffer:public Buffer
{
    VkFormat format;                    ///<数据格式
    uint32_t stride;                    ///<单个数据字节数
    uint32_t count;                     ///<数据数量

private:

    friend class Device;

    VertexAttribBuffer(VkDevice d,const BufferData &vb,VkFormat fmt,uint32_t _stride,uint32_t _count):Buffer(d,vb)
    {
        format=fmt;
        stride=_stride;
        count=_count;
    }

public:

    ~VertexAttribBuffer()=default;

    const VkFormat GetFormat()const { return format; }
    const uint32_t GetStride()const { return stride; }
    const uint32_t GetCount ()const { return count; }

    void *Map(VkDeviceSize start=0,VkDeviceSize size=0) override
    {
        return Buffer::Map(start*stride,size*stride);
    }
};//class VertexAttribBuffer:public Buffer

class IndexBuffer:public Buffer
{
    VkIndexType index_type;
    uint32_t count;

private:

    friend class Device;

    IndexBuffer(VkDevice d,const BufferData &vb,VkIndexType it,uint32_t _count):Buffer(d,vb)
    {
        index_type=it;
        count=_count;
    }

public:

    ~IndexBuffer()=default;

    const VkIndexType   GetType ()const{return index_type;}
    const uint32        GetCount()const{return count;}
};//class IndexBuffer:public Buffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_BUFFER_INCLUDE
