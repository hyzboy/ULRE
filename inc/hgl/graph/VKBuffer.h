#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKMemory.h>
#include<hgl/graph/VKStagedBuffer.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

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
    StagedBuffer *staged_buffer=nullptr;
    VkDeviceSize staged_map_offset=0;
    VkDeviceSize staged_map_size=0;
    bool staged_map_active=false;

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

    DeviceBuffer(VkDevice d,const DeviceBufferData &b,StagedBuffer *sb)
    {
        device=d;
        buf=b;
        staged_buffer=sb;
    }

public:

    virtual ~DeviceBuffer();

            VkBuffer                    GetBuffer       ()const{return buf.buffer;}
            DeviceMemory *              GetMemory       ()const{return buf.memory;}
            VkDeviceMemory              GetVkMemory     ()const{return buf.memory->operator VkDeviceMemory();}
    const   VkDescriptorBufferInfo *    GetBufferInfo   ()const{return &buf.info;}

            void *  Map     ()
            {
                if(staged_buffer)
                {
                    staged_map_offset=0;
                    staged_map_size=VK_WHOLE_SIZE;
                    staged_map_active=true;
                    return staged_buffer->Map();
                }

                return buf.memory?buf.memory->Map():nullptr;
            }
    virtual void *  Map     (VkDeviceSize start,VkDeviceSize size)
            {
                if(staged_buffer)
                {
                    staged_map_offset=start;
                    staged_map_size=(size==0)?VK_WHOLE_SIZE:size;
                    staged_map_active=true;
                    return staged_buffer->Map(start,size);
                }

                return buf.memory?buf.memory->Map(start,size):nullptr;
            }
            void    Unmap   ()
            {
                if(staged_buffer)
                {
                    staged_buffer->Unmap();
                    if(staged_map_active)
                    {
                        staged_buffer->MarkDirty(staged_map_offset,staged_map_size);
                        staged_map_active=false;
                    }
                    return;
                }

                if(buf.memory)
                    buf.memory->Unmap();
            }
    virtual void    Flush   (VkDeviceSize start,VkDeviceSize size)
            {
                if(staged_buffer)
                {
                    staged_buffer->MarkDirty(start,(size==0)?VK_WHOLE_SIZE:size);
                    return;
                }

                if(buf.memory)
                    buf.memory->Flush(start,size);
            }
    virtual void    Flush   (VkDeviceSize size)
            {
                if(staged_buffer)
                {
                    staged_buffer->MarkDirty(0,(size==0)?VK_WHOLE_SIZE:size);
                    return;
                }

                if(buf.memory)
                    buf.memory->Flush(size);
            }

    virtual bool    Write   (const void *ptr,uint32_t start,uint32_t size)
            {
                if(staged_buffer)
                    return staged_buffer->Write(ptr,start,size);

                return buf.memory?buf.memory->Write(ptr,start,size):false;
            }
    virtual bool    Write   (const void *ptr,uint32_t size)
            {
                if(staged_buffer)
                    return staged_buffer->Write(ptr,0,size);

                return buf.memory?buf.memory->Write(ptr,0,size):false;
            }
            bool    Write   (const void *ptr)
            {
                if(staged_buffer)
                    return staged_buffer->Write(ptr);

                return buf.memory?buf.memory->Write(ptr):false;
            }
};//class DeviceBuffer

template<typename T> class DeviceBufferMap
{
protected:

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

    bool Write(const void *data,const uint32_t offset,const uint32_t size)
    {
        if(!dev_buf)
            return(false);

        return dev_buf->Write(data,offset,size);
    }

    void Update()const
    {
        if(dev_buf)
            dev_buf->Write(&data_map,sizeof(T));
    }
};//template<typename T> class DeviceBufferMap

template<typename T> class UBOInstance:public DeviceBufferMap<T>
{
    DescriptorSetType desc_set_type;
    AnsiString ubo_name;

public:

    const DescriptorSetType &   set_type()const{return desc_set_type;}
    const AnsiString &          name    ()const{return ubo_name;}
    DeviceBuffer *              ubo     ()const{return this->dev_buf;}

public:

    UBOInstance(DeviceBuffer *buf,const DescriptorSetType dst,const AnsiString &n):DeviceBufferMap<T>(buf)
    {
        desc_set_type=dst;
        ubo_name=n;
    }

    UBOInstance(DeviceBuffer *buf,const ShaderBufferDesc *desc):DeviceBufferMap<T>(buf)
    {
        desc_set_type=desc->set_type;
        ubo_name=desc->name;
    }
};//template<typename T> class UBOInstance:public DeviceBufferMap<T>

VK_NAMESPACE_END
