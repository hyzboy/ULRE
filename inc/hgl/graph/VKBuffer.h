#ifndef HGL_GRAPH_VULKAN_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_BUFFER_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKMemory.h>
VK_NAMESPACE_BEGIN
struct DeviceBufferData
{
    VkBuffer                buffer=nullptr;
    DeviceMemory *          memory=nullptr;
    VkDescriptorBufferInfo  info;
};//struct DeviceBufferData

class DeviceBuffer
{
protected:

    VkDevice device;
    DeviceBufferData buf;

private:

    friend class GPUDevice;
    friend class VertexAttribBuffer;
    friend class IndexBuffer;

    DeviceBuffer(VkDevice d,const DeviceBufferData &b)
    {
        device=d;
        buf=b;
    }

public:

    virtual ~DeviceBuffer();

            VkBuffer                    GetBuffer       ()const{return buf.buffer;}
            DeviceMemory *              GetMemory       ()const{return buf.memory;}
    const   VkDescriptorBufferInfo *    GetBufferInfo   ()const{return &buf.info;}

            void *  Map     ()                                              {return buf.memory->Map();}
    virtual void *  Map     (VkDeviceSize start,VkDeviceSize size)          {return buf.memory->Map(start,size);}
            void    Unmap   ()                                              {return buf.memory->Unmap();}
            void    Flush   (VkDeviceSize start,VkDeviceSize size)          {return buf.memory->Flush(start,size);}
            void    Flush   (VkDeviceSize size)                             {return buf.memory->Flush(size);}

            bool    Write   (const void *ptr,uint32_t start,uint32_t size)  {return buf.memory->Write(ptr,start,size);}
            bool    Write   (const void *ptr,uint32_t size)                 {return buf.memory->Write(ptr,0,size);}
            bool    Write   (const void *ptr)                               {return buf.memory->Write(ptr);}
};//class DeviceBuffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_BUFFER_INCLUDE
