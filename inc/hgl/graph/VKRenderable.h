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
struct VertexInputData
{
    uint32_t vab_count;
    VkBuffer *vab_list;
    IndexBuffer *ibo;

public:

    VertexInputData(const uint32_t,const uint32_t,const IBAccess *iba);
    ~VertexInputData();

    const bool Comp(const VertexInputData *vid)const;
};//struct VertexInputData

struct DrawData
{
    uint            vab_count;
    VkDeviceSize *  vab_offset;
    VkDeviceSize    vertex_count;

    VkDeviceSize    index_start;
    VkDeviceSize    index_count;

public:

    DrawData(const uint32_t bc,const VkDeviceSize vc,const IBAccess *iba);
    ~DrawData();

    const bool Comp(const DrawData *)const;
};

/**
* 可渲染对象<br>
*/
class Renderable                                                                ///可渲染对象实例
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    Primitive *         primitive;

    VertexInputData *   vertex_input_data;
    DrawData *          draw_data;

private:

    friend Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);

    Renderable(Primitive *,MaterialInstance *,Pipeline *,VertexInputData *,DrawData *);

public:

    virtual ~Renderable()
    {
        //需要在这里添加删除pipeline/desc_sets/primitive引用计数的代码

        SAFE_CLEAR(vertex_input_data);
        SAFE_CLEAR(draw_data);
    }

            void                UpdatePipeline      (Pipeline *p){pipeline=p;}

            Pipeline *          GetPipeline         (){return pipeline;}
            VkPipelineLayout    GetPipelineLayout   (){return mat_inst->GetMaterial()->GetPipelineLayout();}
            Material *          GetMaterial         (){return mat_inst->GetMaterial();}
            MaterialInstance *  GetMaterialInstance (){return mat_inst;}
            Primitive *         GetPrimitive        (){return primitive;}
    const   AABB &              GetBoundingBox      ()const{return primitive->GetBoundingBox();}

    const   VertexInputData *   GetVertexInputData  ()const{return vertex_input_data;}
    const   DrawData *          GetDrawData         ()const{return draw_data;}
};//class Renderable

Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDERABLE_INCLUDE
