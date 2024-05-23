#pragma once

#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

VK_NAMESPACE_BEGIN
/**
 * 可绘制图元创建器
 */
class PrimitiveCreater
{
protected:

    GPUDevice *device;
    const GPUPhysicalDevice *phy_device;

    const VIL *         vil;

protected:

    AnsiString        prim_name;        ///<名称
    PrimitiveData *   prim_data;        ///<图元数据
    
    VkDeviceSize      vertices_number;  ///<顶点数量
    uint              vab_proc_count;   ///<操作的vab数量

    VkDeviceSize      index_number;     ///<索引数量
    IBAccess *        iba;

protected:

    void ClearAllData();

public:

    PrimitiveCreater(GPUDevice *,const VIL *,const AnsiString &name);
    PrimitiveCreater(VertexDataManager *);
    virtual ~PrimitiveCreater()
    {
        ClearAllData();
    }

    virtual bool                    Init(const VkDeviceSize vertices_count,const VkDeviceSize index_count,IndexType it=IndexType::AUTO);                 ///<初始化，参数为顶点数量
    
            const   VkDeviceSize    GetVertexCount()const{ return vertices_number; }                            ///<取得顶点数量

                    VABAccess *     AcquirePVB  (const AnsiString &name,const VkFormat &format,const void *data=nullptr,const VkDeviceSize bytes=0);           ///<请求一个顶点属性数据区
                    bool            WriteVAB    (const AnsiString &name,const VkFormat &format,const void *data,const uint32_t bytes)     ///<直接写入顶点属性数据
                    {
                        return AcquirePVB(name,format,data,bytes);
                    }

            const   IndexType       GetIndexType()const{return iba->buffer->GetType();}
                    IBAccess *      AcquireIBO(){return iba;}

                    bool            WriteIBO(const void *data,const VkDeviceSize bytes);

                    template<typename T>
                    bool            WriteIBO(const T *data){return WriteIBO(data,index_number*sizeof(T));}

    virtual Primitive *             Finish(RenderResource *);                                                                   ///<结束并创建可渲染对象
};//class PrimitiveCreater

/**
* VAB原生数据访问映射
*/
template<typename T> class VABRawMap
{
    VABAccess *vaba;
    T *map_ptr;

public:

    VABRawMap(PrimitiveCreater *pc,const VkFormat &format,const AnsiString &name)
    {
        vaba=pc->AcquirePVB(name,format);

        if(vaba)
            map_ptr=(T *)(vaba->vab->Map(vaba->start,vaba->count));
        else
            map_ptr=nullptr;
    }

    ~VABRawMap()
    {
        if(vaba)
            vaba->vab->Unmap();
    }

    const bool IsValid()const{ return vaba; }

    operator T *(){ return map_ptr; }
};//template<typename T> class VABRawMap

typedef VABRawMap<int8>   VABRawMapi8, VABRawMapByte;
typedef VABRawMap<int16>  VABRawMapi16,VABRawMapShort;
typedef VABRawMap<int32>  VABRawMapi32,VABRawMapInt;
typedef VABRawMap<uint8>  VABRawMapu8, VABRawMapUByte;
typedef VABRawMap<uint16> VABRawMapu16,VABRawMapUShort;
typedef VABRawMap<uint32> VABRawMapu32,VABRawMapUInt;
typedef VABRawMap<float>  VABRawMapFloat;
typedef VABRawMap<double> VABRawMapDouble;

/**
* VAB VertexAttribDataAccess数据访问映射
*/
template<typename T> class VABMap
{
    VABAccess *vaba;
    T *vb;

public:

    VABMap(PrimitiveCreater *pc,const AnsiString &name)
    {
        vaba=pc->AcquirePVB(name,T::GetVulkanFormat(),nullptr);

        if(vaba)
        {
            void *map_ptr=vaba->vab->Map(vaba->start,vaba->count);

            vb=T::Create(pc->GetVertexCount(),map_ptr);

            vb->Begin();
        }
        else
        {
            vb=nullptr;
        }
    }

    ~VABMap()
    {
        if(vaba)
            vaba->vab->Unmap();
    }

    void Restart()
    {
        if(vb)
            vb->Begin();
    }

    const bool IsValid()const{ return vb; }

    T *operator->(){ return vb; }
};//template<typename T> class VABMap

typedef VABMap<VB1i8>   VABMap1i8 ,VABMap1b;
typedef VABMap<VB1i16>  VABMap1i16,VABMap1s;
typedef VABMap<VB1i32>  VABMap1i32,VABMap1i;
typedef VABMap<VB1u8>   VABMap1u8 ,VABMap1ub;
typedef VABMap<VB1u16>  VABMap1u16,VABMap1us;
typedef VABMap<VB1u32>  VABMap1u32,VABMap1ui;
typedef VABMap<VB1f>    VABMap1f;
typedef VABMap<VB1d>    VABMap1d;

typedef VABMap<VB2i8>   VABMap2i8 ,VABMap2b;
typedef VABMap<VB2i16>  VABMap2i16,VABMap2s;
typedef VABMap<VB2i32>  VABMap2i32,VABMap2i;
typedef VABMap<VB2u8>   VABMap2u8 ,VABMap2ub;
typedef VABMap<VB2u16>  VABMap2u16,VABMap2us;
typedef VABMap<VB2u32>  VABMap2u32,VABMap2ui;
typedef VABMap<VB2f>    VABMap2f;
typedef VABMap<VB2d>    VABMap2d;

typedef VABMap<VB3i32>  VABMap3i32,VABMap3i;
typedef VABMap<VB3u32>  VABMap3u32,VABMap3ui;
typedef VABMap<VB3f>    VABMap3f;
typedef VABMap<VB3d>    VABMap3d;

typedef VABMap<VB4i8>   VABMap4i8 ,VABMap4b;
typedef VABMap<VB4i16>  VABMap4i16,VABMap4s;
typedef VABMap<VB4i32>  VABMap4i32,VABMap4i;
typedef VABMap<VB4u8>   VABMap4u8, VABMap4ub;
typedef VABMap<VB4u16>  VABMap4u16,VABMap4us;
typedef VABMap<VB4u32>  VABMap4u32,VABMap4ui;
typedef VABMap<VB4f>    VABMap4f;
typedef VABMap<VB4d>    VABMap4d;

/**
* 索引缓冲区映射访问
*/
template<typename T> class IBMap
{
    IBAccess *iba;
    T *map_ptr;

public:

    IBMap(IBAccess *a)
    {
        iba=a;

        if(iba)
            map_ptr=(T *)(iba->buffer->Map(iba->start,iba->count));
        else
            map_ptr=nullptr;
    }

    IBMap(PrimitiveCreater *pc):IBMap(pc->AcquireIBO()){}

    ~IBMap()
    {
        if(iba)
            iba->buffer->Unmap();
    }

    const bool IsValid()const{ return iba; }

    operator T *(){ return map_ptr; }
};//template<typename T> class IBMap

using IBMapU8=IBMap<uint8>;
using IBMapU16=IBMap<uint16>;
using IBMapU32=IBMap<uint32>;

VK_NAMESPACE_END