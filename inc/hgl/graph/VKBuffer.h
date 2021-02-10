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

private:

    friend class GPUDevice;
    friend class VertexAttribBuffer;
    friend class IndexBuffer;

    GPUBuffer(VkDevice d,const GPUBufferData &b)
    {
        device=d;
        buf=b;
    }

public:

    virtual ~GPUBuffer();

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
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_BUFFER_INCLUDE
