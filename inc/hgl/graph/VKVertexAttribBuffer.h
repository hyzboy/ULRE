﻿#pragma once

#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKBufferMap.h>

VK_NAMESPACE_BEGIN
class VertexAttribBuffer:public DeviceBuffer
{
    VkFormat format;                    ///<数据格式
    uint32_t stride;                    ///<单个数据字节数
    uint32_t count;                     ///<数据数量

private:

    friend class GPUDevice;

    VertexAttribBuffer(VkDevice d,const DeviceBufferData &vb,VkFormat fmt,uint32_t _stride,uint32_t _count):DeviceBuffer(d,vb)
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

    const VkDeviceSize GetTotalBytes()const { return stride*count; }
            
public:

    void *  Map     (VkDeviceSize start,VkDeviceSize size)          override {return DeviceBuffer::Map(start*stride,size*stride);}
    void    Flush   (VkDeviceSize start,VkDeviceSize size)          override {return DeviceBuffer::Flush(start*stride,size*stride);}
    void    Flush   (VkDeviceSize size)                             override {return DeviceBuffer::Flush(size*stride);}
            
    bool    Write   (const void *ptr,uint32_t start,uint32_t size)  override {return DeviceBuffer::Write(ptr,start*stride,size*stride);}
    bool    Write   (const void *ptr,uint32_t size)                 override {return DeviceBuffer::Write(ptr,0,size*stride);}
};//class VertexAttribBuffer:public DeviceBuffer

using VAB=VertexAttribBuffer;
        
class VABMap:public VKBufferMap<VAB>
{
public:

    using VKBufferMap<VAB>::VKBufferMap;
    virtual ~VABMap()=default;

    const VkFormat GetFormat()const { return buffer->GetFormat(); }

    void SetVAB(VAB *vab,const VkDeviceSize off,const uint32_t count)
    {
        VKBufferMap<VAB>::Set(vab,off,vab->GetStride(),count);
    }
};//class VABMap

/**
* 顶点属性缓冲区原生数据访问映射
*/
template<typename T> class VABRawMap
{
    VABMap *vab_map;
    T *map_ptr;

public:

    VABRawMap(VABMap *map,const VkFormat check_format=VK_FORMAT_UNDEFINED,bool now_map=true)
    {
        vab_map=map;
        map_ptr=nullptr;

        if(vab_map)
        {
            if(check_format==VK_FORMAT_UNDEFINED
             ||check_format==vab_map->GetFormat())
            {
                if(now_map)
                    map_ptr=(T *)(vab_map->Map());
            }
            else
            {
                vab_map=nullptr;
            }
        }
    }

    ~VABRawMap()
    {
        if(map_ptr)
            vab_map->Unmap();
    }

    const bool IsValid()const{ return vab_map?vab_map->IsValid():false; }

    T *Map()
    {
        if(!vab_map)
            return(nullptr);

        if(!map_ptr)
            map_ptr=(T *)(vab_map->Map());

        return map_ptr;
    }

    void Unmap()
    {
        if(vab_map)
        {
            if(map_ptr)
                vab_map->Unmap();
        }
    }

    operator T *(){ return map_ptr; }
    T *operator->(){ return map_ptr; }
};//template<typename T> class VABRawMap

typedef VABRawMap<int8>    VABMapI8,   VABMapByte;
typedef VABRawMap<int16>   VABMapI16,  VABMapShort;
typedef VABRawMap<int32>   VABMapI32,  VABMapInt;
typedef VABRawMap<uint8>   VABMapU8,   VABMapUByte;
typedef VABRawMap<uint16>  VABMapU16,  VABMapUShort;
typedef VABRawMap<uint32>  VABMapU32,  VABMapUInt;
typedef VABRawMap<float>   VABMapFloat;
typedef VABRawMap<double>  VABMapDouble;
typedef VABRawMap<half_float>   VABMapHalfFloat;

/**
* 顶点属性缓冲区数据访问映射
*/
template<typename T> class VABFormatMap
{
    VABMap *vab_map;

    T *map_ptr;

public:

    VABFormatMap(VABMap *map,bool now_map=true)
    {   
        vab_map=map;

        if(vab_map&&vab_map->GetFormat()==T::GetVulkanFormat())
        {
            if(now_map)
            {
                map_ptr=T::Create(vab_map->GetCount(),vab_map->Map());
                map_ptr->Begin();
            }
            else
            {
                map_ptr=T::Create(vab_map->GetCount(),nullptr);
            }
        }
        else
            map_ptr=nullptr;
    }

    ~VABFormatMap()
    {
        if(map_ptr)
        {
            vab_map->Unmap();
            delete map_ptr;
        }
    }

    const bool IsValid()const{ return map_ptr?map_ptr->IsValid():false; }
    
    T *Map()
    {
        if(!vab_map)
            return(nullptr);

        if(!map_ptr)
        {
            map_ptr=T::Create(vab_map->GetCount(),vab_map->Map());
        }
        else
        {
            if(!map_ptr->IsValid())
                map_ptr->SetData(vab_map->Map());
        }
        
        map_ptr->Begin();
        return map_ptr;
    }

    void Unmap()
    {
        if(vab_map)
        {
            if(map_ptr&&map_ptr->IsValid())
            {
                vab_map->Unmap();
                map_ptr->SetData(nullptr);
            }
        }
    }

    void Restart()
    {
        return Map();
    }

    T *operator->()
    {
        return map_ptr;
    }
};//template<typename T> class VABFormatMap

typedef VABFormatMap<VB1i8>   VABMap1i8 ,VABMap1b;
typedef VABFormatMap<VB1i16>  VABMap1i16,VABMap1s;
typedef VABFormatMap<VB1i32>  VABMap1i32,VABMap1i;

typedef VABFormatMap<VB1u8>   VABMap1u8 ,VABMap1ub;
typedef VABFormatMap<VB1u16>  VABMap1u16,VABMap1us;
typedef VABFormatMap<VB1u32>  VABMap1u32,VABMap1ui;

typedef VABFormatMap<VB1f>    VABMap1f;
typedef VABFormatMap<VB1d>    VABMap1d;

typedef VABFormatMap<VB1sf8>  VABMap1sf8;
typedef VABFormatMap<VB1uf8>  VABMap1uf8;
typedef VABFormatMap<VB1sf16> VABMap1sf16;
typedef VABFormatMap<VB1uf16> VABMap1uf16;
typedef VABFormatMap<VB1hf>   VABMap1hf;

typedef VABFormatMap<VB2i8>   VABMap2i8 ,VABMap2b;
typedef VABFormatMap<VB2i16>  VABMap2i16,VABMap2s;
typedef VABFormatMap<VB2i32>  VABMap2i32,VABMap2i;
typedef VABFormatMap<VB2u8>   VABMap2u8 ,VABMap2ub;
typedef VABFormatMap<VB2u16>  VABMap2u16,VABMap2us;
typedef VABFormatMap<VB2u32>  VABMap2u32,VABMap2ui;
typedef VABFormatMap<VB2f>    VABMap2f;
typedef VABFormatMap<VB2d>    VABMap2d;

typedef VABFormatMap<VB2sf8>  VABMap2sf8;
typedef VABFormatMap<VB2uf8>  VABMap2uf8;
typedef VABFormatMap<VB2sf16> VABMap2sf16;
typedef VABFormatMap<VB2uf16> VABMap2uf16;
typedef VABFormatMap<VB2hf>   VABMap2hf;

typedef VABFormatMap<VB3i32>  VABMap3i32,VABMap3i;
typedef VABFormatMap<VB3u32>  VABMap3u32,VABMap3ui;
typedef VABFormatMap<VB3f>    VABMap3f;
typedef VABFormatMap<VB3d>    VABMap3d;

typedef VABFormatMap<VB4i8>   VABMap4i8 ,VABMap4b;
typedef VABFormatMap<VB4i16>  VABMap4i16,VABMap4s;
typedef VABFormatMap<VB4i32>  VABMap4i32,VABMap4i;
typedef VABFormatMap<VB4u8>   VABMap4u8, VABMap4ub;
typedef VABFormatMap<VB4u16>  VABMap4u16,VABMap4us;
typedef VABFormatMap<VB4u32>  VABMap4u32,VABMap4ui;
typedef VABFormatMap<VB4f>    VABMap4f;
typedef VABFormatMap<VB4d>    VABMap4d;

typedef VABFormatMap<VB4sf8>  VABMap4sf8;
typedef VABFormatMap<VB4uf8>  VABMap4uf8;
typedef VABFormatMap<VB4sf16> VABMap4sf16;
typedef VABFormatMap<VB4uf16> VABMap4uf16;
typedef VABFormatMap<VB4hf>   VABMap4hf;

VK_NAMESPACE_END
