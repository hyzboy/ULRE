#pragma once

#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/common/VertexAttribDef.h>

namespace hgl::graph{

class BufferManager;
/*
    1.截止2024.4.27，根据vulkan.gpuinfo.org统计，只有9%的设备maxVertexInputAttributes为16，不存在低于16的设备。
         9.0%的设备为28 - 31
        70.7%的设备为32
         9.6%的设备为64

        由于我们暂时没有发现需要使用16个以上属性的情况，所以这里暂定使用16。
        (如果时间过去久远，可再次查询此值是否可改成更高的值，以及是否需要)

    2.为何va_name使用char[][]而不是String以及动态分配内存？

        就是为了必避动态分配内存，以及可以直接memcpy处理，所以此处这样定义。
*/

class GeometryData
{
protected:

    const VIL *     vil;

    uint32_t        vertex_count;
    uint32_t        index_count;

    VAB **          vab_list;

    virtual VAB * CreateVAB(const int vab_index,const VkFormat format,const void *data,const AnsiString &name)=0;

protected:

    IndexBuffer *   ibo;

    virtual IndexBuffer *CreateIBO(const uint32_t ic,const IndexType &it,const AnsiString &name)=0;

public:

    GeometryData(const VIL *_vil,const uint32_t vc);
    virtual ~GeometryData();

public:

    const   uint32_t        GetVertexCount  ()const{return vertex_count;}
    const   uint32_t        GetVABCount     ()const;
    const   int             GetVABIndex     (const VertexAttrib)const;

            bool            CreateAllVAB(const AnsiString &geometry_name="Geometry");     //根据VIL创建所有VAB

            VAB *           GetVABByIndex   (const int index)const;
            VAB *           GetVABByAttrib  (const VertexAttrib)const;

            VAB *           InitVAB         (const int vab_index,const void *data,const AnsiString &name="VAB");

            IndexBuffer *   InitIBO         (const int index_count,IndexType it,const AnsiString &name="IBO");
            IndexBuffer *   GetIBO          (){return ibo;}
            uint32_t        GetIndexCount   ()const{return index_count;}

    virtual int32_t         GetVertexOffset ()const=0;                      ///<取得顶点偏移(注意是顶点不是字节)
    virtual uint32_t        GetFirstIndex   ()const=0;                      ///<取得第一个索引

    virtual VertexDataManager * GetVDM()const=0;                            ///<取得顶点数据管理器

            void            UnmapAll();

};//class GeometryData

GeometryData *CreateGeometryData(VulkanDevice *dev,const VIL *_vil,const uint32_t vc);
GeometryData *CreateGeometryData(VulkanDevice *dev,const VIL *_vil,const uint32_t vc,BufferAllocPolicy policy);
GeometryData *CreateGeometryData(BufferManager *bm,const VIL *_vil,const uint32_t vc);
GeometryData *CreateGeometryData(BufferManager *bm,const VIL *_vil,const uint32_t vc,BufferAllocPolicy policy);
GeometryData *CreateGeometryData(VertexDataManager *vdm,const uint32_t vc);
}//namespace hgl::graph
