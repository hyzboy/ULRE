#ifndef HGL_GRAPH_RENDERABLE_INCLUDE
#define HGL_GRAPH_RENDERABLE_INCLUDE

#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VertexAttrib.h>
VK_NAMESPACE_BEGIN
struct PrimitiveRenderBuffer
{
    uint32_t        vab_count;
    VkBuffer *      vab_list;
    IndexBuffer *   ibo;

public:

    PrimitiveRenderBuffer(const uint32_t,const uint32_t,const IBAccess *iba);
    ~PrimitiveRenderBuffer();

    const bool Comp(const PrimitiveRenderBuffer *prb)const;
};//struct PrimitiveRenderBuffer

struct PrimitiveRenderData
{
    uint            vab_count;
    VkDeviceSize *  vab_offset;
    uint32_t        vertex_count;

    uint32_t        index_start;
    uint32_t        index_count;

public:

    PrimitiveRenderData(const uint32_t bc,const uint32_t vc,const IBAccess *iba);
    ~PrimitiveRenderData();

    const bool Comp(const PrimitiveRenderData *)const;
};

/**
* 可渲染对象<br>
*/
class Renderable                                                                ///可渲染对象实例
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    Primitive *         primitive;

    PrimitiveRenderBuffer * primitive_render_buffer;
    PrimitiveRenderData *   primitive_render_data;

private:

    friend Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);

    Renderable(Primitive *,MaterialInstance *,Pipeline *,PrimitiveRenderBuffer *,PrimitiveRenderData *);

public:

    virtual ~Renderable()
    {
        //需要在这里添加删除pipeline/desc_sets/primitive引用计数的代码

        SAFE_CLEAR(primitive_render_buffer);
        SAFE_CLEAR(primitive_render_data);
    }

            void                UpdatePipeline      (Pipeline *p){pipeline=p;}

            Pipeline *          GetPipeline         (){return pipeline;}
            VkPipelineLayout    GetPipelineLayout   (){return mat_inst->GetMaterial()->GetPipelineLayout();}
            Material *          GetMaterial         (){return mat_inst->GetMaterial();}
            MaterialInstance *  GetMaterialInstance (){return mat_inst;}
            Primitive *         GetPrimitive        (){return primitive;}
    const   AABB &              GetBoundingBox      ()const{return primitive->GetBoundingBox();}

    const   PrimitiveRenderBuffer * GetRenderBuffer ()const{return primitive_render_buffer;}
    const   PrimitiveRenderData *   GetRenderData   ()const{return primitive_render_data;}
};//class Renderable

Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDERABLE_INCLUDE
