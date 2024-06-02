#pragma once

#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VertexAttrib.h>
VK_NAMESPACE_BEGIN
/**
* 原始图元数据缓冲区<Br>
* 提供在渲染之前的数据绑定信息
*/
struct PrimitiveDataBuffer
{
    uint32_t        vab_count;
    VkBuffer *      vab_list;

    // 理论上讲，每个VAB绑定时都是可以指定byte offsets的。但是随后Draw时，又可以指定vertexOffset。
    // 在我们支持的两种draw模式中，一种是每个模型一批VAB，所有VAB的offset都是0。
    // 另一种是使用VDM，为了批量渲染，所有的VAB又必须对齐，所以每个VAB单独指定offset也不可行。

    VkDeviceSize *  vab_offset;         //注意：这里的offset是字节偏移，不是顶点偏移

    IndexBuffer *   ibo;

    VertexDataManager *vdm;             //只是用来区分和比较的，不实际使用

public:

    PrimitiveDataBuffer(const uint32_t,IndexBuffer *,VertexDataManager *_v=nullptr);
    ~PrimitiveDataBuffer();

    const bool Comp(const PrimitiveDataBuffer *pdb)const;
};//struct PrimitiveDataBuffer

/**
* 原始图元渲染数据<Br>
* 提供在渲染时的数据
*/
struct PrimitiveRenderData
{
    //因为要VAB是流式访问，所以我们这个结构会被用做排序依据
    //也因此，把vertex_offset放在最前面

    int32_t         vertex_offset;  //注意：这里的offset是相对于vertex的，代表第几个顶点，不是字节偏移
    uint32_t        first_index;

    uint32_t        vertex_count;
    uint32_t        index_count;

public:

    PrimitiveRenderData(const uint32_t vc,const uint32_t ic,const int32_t vo=0,const uint32_t fi=0)
    {
        vertex_count    =vc;
        index_count     =ic;
        vertex_offset   =vo;
        first_index     =fi;
    }

    CompOperatorMemcmp(const PrimitiveRenderData &);
    CompOperatorMemcmpPointer(PrimitiveRenderData);
};

/**
* 原始可渲染对象(即仅一个模型一个材质)
*/
class Renderable                                                                ///可渲染对象实例
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    Primitive *         primitive;

    PrimitiveDataBuffer * primitive_data_buffer;
    PrimitiveRenderData * primitive_render_data;

private:

    friend Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);

    Renderable(Primitive *,MaterialInstance *,Pipeline *,PrimitiveDataBuffer *,PrimitiveRenderData *);

public:

    virtual ~Renderable()
    {
        //需要在这里添加删除pipeline/desc_sets/primitive引用计数的代码

        SAFE_CLEAR(primitive_data_buffer);
        SAFE_CLEAR(primitive_render_data);
    }

            void                UpdatePipeline      (Pipeline *p){pipeline=p;}

            Pipeline *          GetPipeline         (){return pipeline;}
            VkPipelineLayout    GetPipelineLayout   (){return mat_inst->GetMaterial()->GetPipelineLayout();}
            Material *          GetMaterial         (){return mat_inst->GetMaterial();}
            MaterialInstance *  GetMaterialInstance (){return mat_inst;}
            Primitive *         GetPrimitive        (){return primitive;}
    const   AABB &              GetBoundingBox      ()const{return primitive->GetBoundingBox();}

    const   PrimitiveDataBuffer *GetDataBuffer      ()const{return primitive_data_buffer;}
    const   PrimitiveRenderData *GetRenderData      ()const{return primitive_render_data;}
};//class Renderable

Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
