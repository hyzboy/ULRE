#pragma once

#include<hgl/graph/VKBufferMap.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

VK_NAMESPACE_BEGIN
/**
 * 可绘制原始图形创建器
 */
class PrimitiveCreater
{
protected:

    GPUDevice *         device;
    VertexDataManager * vdm;

    const VIL *         vil;

protected:

    AnsiString      prim_name;
    PrimitiveData * prim_data;

    uint32_t        vertices_number;  ///<顶点数量

    void_pointer   *map_ptr_list;     ///<映射指针列表

    uint32_t        index_number;     ///<索引数量
    IndexType       index_type;       ///<索引类型
    IndexBuffer *   ibo;              ///<索引缓冲区

public:

    PrimitiveCreater(GPUDevice *,const VIL *);
    PrimitiveCreater(VertexDataManager *);
    virtual ~PrimitiveCreater();

            /**
            * 初始化一个原始图形创建
            * @parama name              原始图形名称
            * @parama vertices_count    顶点数量
            * @parama index_count       索引数量
            * @parama it                索引类型(注：当使用VDM时，此值无效)
            */
            bool            Init(const AnsiString &name,
                                 const uint32_t vertices_count,
                                 const uint32_t index_count=0,IndexType it=IndexType::AUTO);

            void            Clear();                                                                                        ///<清除创建器数据

public: //顶点缓冲区

    const   uint32_t        GetVertexCount()const{ return vertices_number; }                                                ///<取得顶点数量

    const   int             GetVABIndex (const AnsiString &name,const VkFormat &format);                                    ///<取得顶点属性索引 

            void *          MapVAB      (const int vab_index);                                                              ///<映射一个顶点属性数据区
            void            UnmapVAB    (const int vab_index);                                                              ///<取消映射

            bool            WriteVAB    (const AnsiString &name,const VkFormat &format,const void *data);                   ///<直接写入顶点属性数据

public: //索引缓冲区

    const   IndexType       GetIndexType()const{return index_type;}                                                         ///<取得索引类型
    const   uint32_t        GetIndexCount()const{return index_number;}                                                      ///<取得索引数量

            void *          MapIBO();
            void            UnmapIBO();

            bool            WriteIBO(const void *data,const uint32_t count);

            template<typename T>
            bool            WriteIBO(const T *data){return WriteIBO(data,index_number);}

public: //创建可渲染对象

            Primitive *     Create();                                                                                       ///<创建一个可渲染对象，并清除创建器数据
};//class PrimitiveCreater

/**
* VAB原生数据访问映射
*/
template<typename T> class VABRawMap
{
    PrimitiveCreater *pc;
    int vab_index;
    T *map_ptr;

public:

    VABRawMap(PrimitiveCreater *c,const VkFormat &format,const AnsiString &name)
    {
        pc=c;
        vab_index=pc->GetVABIndex(name,format);
        
        map_ptr=(T *)(pc->MapVAB(vab_index));
    }

    ~VABRawMap()
    {
        if(map_ptr)
            pc->UnmapVAB(vab_index);
    }

    const bool IsValid()const{ return map_ptr; }

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
    PrimitiveCreater *pc;
    int vab_index;
    T *vb;

public:

    VABMap(PrimitiveCreater *c,const AnsiString &name)
    {
        pc=c;
        vab_index=pc->GetVABIndex(name,T::GetVulkanFormat());

        void *map_ptr=(T *)(pc->MapVAB(vab_index));

        if(map_ptr)
        {
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
        if(pc&&vab_index>=0)
            pc->UnmapVAB(vab_index);
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
    PrimitiveCreater *pc;
    T *map_ptr;

public:

    IBMap(PrimitiveCreater *c)
    {
        pc=c;

        if(pc)
            map_ptr=(T *)(pc->MapIBO());
        else
            map_ptr=nullptr;
    }

    ~IBMap()
    {
        if(map_ptr&&pc)
            pc->UnmapIBO();
    }

    const bool IsValid()const{ return map_ptr; }

    operator T *(){ return map_ptr; }
};//template<typename T> class IBMap

using IBMapU8=IBMap<uint8>;
using IBMapU16=IBMap<uint16>;
using IBMapU32=IBMap<uint32>;

VK_NAMESPACE_END
