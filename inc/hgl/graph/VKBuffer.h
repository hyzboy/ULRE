#ifndef HGL_GRAPH_VULKAN_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_BUFFER_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKMemory.h>
VK_NAMESPACE_BEGIN
struct GPUBufferData
{
    VkBuffer                buffer;
    GPUMemory *             memory=nullptr;
    VkDescriptorBufferInfo  info;
};//struct GPUBufferData

class GPUBuffer
{
protected:

    VkDevice device;
    GPUBufferData buf;

    bool dynamic;

private:

    friend class GPUDevice;
    friend class VertexAttribBuffer;
    friend class IndexBuffer;

    GPUBuffer(VkDevice d,const GPUBufferData &b,bool dy)
    {
        device=d;
        buf=b;
        dynamic=dy;
    }

public:

    virtual ~GPUBuffer();

    const   bool                        IsDynamic       ()const{return dynamic;}
            VkBuffer                    GetBuffer       ()const{return buf.buffer;}
            GPUMemory *                 GetMemory       ()const{return buf.memory;}
    const   VkDescriptorBufferInfo *    GetBufferInfo   ()const{return &buf.info;}

            void *  Map     ()                                              {return buf.memory->Map();}
    virtual void *  Map     (VkDeviceSize start,VkDeviceSize size)          {return buf.memory->Map(start,size);}
            void    Unmap   ()                                              {return buf.memory->Unmap();}
            void    Flush   (VkDeviceSize start,VkDeviceSize size)          {return buf.memory->Flush(start,size);}
            void    Flush   (VkDeviceSize size)                             {return buf.memory->Flush(size);}

            bool    Write   (const void *ptr,uint32_t start,uint32_t size)  {return buf.memory->Write(ptr,start,size);}
            bool    Write   (const void *ptr,uint32_t size)                 {return buf.memory->Write(ptr,0,size);}
            bool    Write   (const void *ptr)                               {return buf.memory->Write(ptr);}
};//class GPUBuffer

class VertexAttribBuffer:public GPUBuffer
{
    VkFormat format;                    ///<数据格式
    uint32_t stride;                    ///<单个数据字节数
    uint32_t count;                     ///<数据数量

private:

    friend class GPUDevice;

    VertexAttribBuffer(VkDevice d,const GPUBufferData &vb,VkFormat fmt,uint32_t _stride,uint32_t _count,bool dy=false):GPUBuffer(d,vb,dy)
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
        return GPUBuffer::Map(start*stride,size*stride);
    }
};//class VertexAttribBuffer:public GPUBuffer

using VAB=VertexAttribBuffer;

class IndexBuffer:public GPUBuffer
{
    IndexType index_type;
    uint32_t count;

private:

    friend class GPUDevice;

    IndexBuffer(VkDevice d,const GPUBufferData &vb,IndexType it,uint32_t _count,bool dy=false):GPUBuffer(d,vb,dy)
    {
        index_type=it;
        count=_count;
    }

public:

    ~IndexBuffer()=default;

    const IndexType GetType ()const{return index_type;}
    const uint32    GetCount()const{return count;}
};//class IndexBuffer:public GPUBuffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_BUFFER_INCLUDE
