#pragma once

#include<hgl/graph/VKBufferMap.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>

VK_NAMESPACE_BEGIN
/**
 * 可绘制原始图形创建器
 */
class PrimitiveCreater
{
protected:

    VulkanDevice *      device;
    VertexDataManager * vdm;

    const VIL *         vil;

protected:

    AnsiString      prim_name;
    PrimitiveData * prim_data;

    uint32_t        vertices_number;  ///<顶点数量

    bool            has_index;        ///<是否有索引
    uint32_t        index_number;     ///<索引数量
    IndexType       index_type;       ///<索引类型
    IndexBuffer *   ibo;              ///<索引缓冲区

protected:
    
    const int InitVAB(const AnsiString &name,const VkFormat format,const void *data);                                       ///<取得顶点属性索引

public:

    PrimitiveCreater(VulkanDevice *,const VIL *);
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

            VABMap *        GetVABMap   (const AnsiString &name,const VkFormat format=VK_FORMAT_UNDEFINED);

            bool            WriteVAB    (const AnsiString &name,const VkFormat format,const void *data);                    ///<直接写入顶点属性数据

public: //索引缓冲区

    const   bool            hasIndex()const{return vdm?has_index:index_number>0;}                                           ///<是否有索引缓冲区
    const   IndexType       GetIndexType()const{return index_type;}                                                         ///<取得索引类型
    const   uint32_t        GetIndexCount()const{return index_number;}                                                      ///<取得索引数量

            IBMap *         GetIBMap();

            bool            WriteIBO(const void *data,const uint32_t count);

            template<typename T>
            bool            WriteIBO(const T *data){return WriteIBO(data,index_number);}

public: //创建可渲染对象

            Primitive *     Create();                                                                                       ///<创建一个可渲染对象，并清除创建器数据
};//class PrimitiveCreater

Primitive *CreatePrimitive(         VulkanDevice *  device,
                            const   VIL *           vil,
                            const   AnsiString &    name,
                            const   uint32_t        vertex_count,
                            const   uint32_t        index_count = 0,
                                    IndexType       it          = IndexType::AUTO);
VK_NAMESPACE_END
