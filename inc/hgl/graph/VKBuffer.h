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

    friend class GPUDevice;
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

template<typename T> struct DeviceBufferData
{
    T *data;                    ///<CPU端数据

    // 数据如何被设置为不可以在CPU端访问，那么不会在CPU端保存备份。
    // 这种情况的话，将不会允许CPU端随机读写，只能写入


    VkDeviceSize size;
    DeviceBuffer *dev_buffer;
};

template<typename T> class DeviceBufferRandomAccess
{
    DeviceBufferData<T> *dbd;

public:

    operator T *(){return dbd->data;}

public:

    DeviceBufferRandomAccess(DeviceBufferData<T> *obj)
    {
        dbd=obj;
            
    }
    virtual ~DeviceBufferAccess()
    {
        if(!dbd)return;
        delete dbd->dev_buffer;
        delete dbd;
    }
    bool Write(const T *ptr)
    {
        return dbd->dev_buffer->Write(ptr);
    }
};

template<typename T> class DeviceBufferObject
{
    DeviceBufferData<T> *dbd;

public:

    DeviceBufferObject(DeviceBufferData<T> *obj)
    {
        dbd=obj;
    }

    virtual ~DeviceBufferObject()
    {
        if(!dbd)return;

        delete dbd->dev_buffer;
        delete dbd;
    }

    

};//template<typename T> class DeviceBufferObject
VK_NAMESPACE_END
