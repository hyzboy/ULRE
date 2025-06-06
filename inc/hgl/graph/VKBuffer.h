#pragma once

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

    friend class VulkanDevice;
    friend class VertexAttribBuffer;
    friend class IndexBuffer;
    template<typename T> friend class IndirectCommandBuffer;

    DeviceBuffer(VkDevice d,const DeviceBufferData &b)
    {
        device=d;
        buf=b;
    }

public:

    virtual ~DeviceBuffer();

            VkBuffer                    GetBuffer       ()const{return buf.buffer;}
            DeviceMemory *              GetMemory       ()const{return buf.memory;}
            VkDeviceMemory              GetVkMemory     ()const{return buf.memory->operator VkDeviceMemory();}
    const   VkDescriptorBufferInfo *    GetBufferInfo   ()const{return &buf.info;}

            void *  Map     ()                                              {return buf.memory->Map();}
    virtual void *  Map     (VkDeviceSize start,VkDeviceSize size)          {return buf.memory->Map(start,size);}
            void    Unmap   ()                                              {return buf.memory->Unmap();}
    virtual void    Flush   (VkDeviceSize start,VkDeviceSize size)          {return buf.memory->Flush(start,size);}
    virtual void    Flush   (VkDeviceSize size)                             {return buf.memory->Flush(size);}

    virtual bool    Write   (const void *ptr,uint32_t start,uint32_t size)  {return buf.memory->Write(ptr,start,size);}
    virtual bool    Write   (const void *ptr,uint32_t size)                 {return buf.memory->Write(ptr,0,size);}
            bool    Write   (const void *ptr)                               {return buf.memory->Write(ptr);}
};//class DeviceBuffer

template<typename T> class DeviceBufferMap
{
    DeviceBuffer *dev_buf;
    T data_map;

public:

    static const VkDeviceSize GetSize()
    {
        return sizeof(T);
    }

public:

    DeviceBufferMap(DeviceBuffer *buf)
    {
        dev_buf=buf;
    }

    virtual ~DeviceBufferMap()
    {
        delete dev_buf;
    }

    DeviceBuffer *GetDeviceBuffer(){return dev_buf;}

    T *data(){return &data_map;}

    void Update()
    {
        if(dev_buf)
            dev_buf->Write(&data_map,sizeof(T));
    }
};//template<typename T> class DeviceBufferMap

VK_NAMESPACE_END
