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
    uint32_t binding_count;
    VkBuffer *buffer_list;
    VkDeviceSize *buffer_offset;

    uint32_t vertex_count;

    const IndexBufferData *index_buffer;

public:

    VertexInputData(const uint32_t,const uint32_t,const IndexBufferData *);
    ~VertexInputData();

    const bool Comp(const VertexInputData *vid)const
    {
        if(binding_count!=vid->binding_count)return(false);

        for(uint32_t i=0;i<binding_count;i++)
        {
            if(buffer_list[i]!=vid->buffer_list[i])return(false);
            if(buffer_offset[i]!=vid->buffer_offset[i])return(false);
        }

        if(vertex_count!=vid->vertex_count)return(false);

        if(index_buffer!=vid->index_buffer)return(false);

        return(true);
    }
};//struct VertexInputData

/**
* 可渲染对象<br>
*/
class Renderable                                                                ///可渲染对象实例
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    Primitive *         primitive;

    VertexInputData *   vertex_input;

private:

    friend Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);

    Renderable(Primitive *,MaterialInstance *,Pipeline *,VertexInputData *);

public:

    virtual ~Renderable();

            void                UpdatePipeline      (Pipeline *p){pipeline=p;}

            Pipeline *          GetPipeline         (){return pipeline;}
            VkPipelineLayout    GetPipelineLayout   (){return mat_inst->GetMaterial()->GetPipelineLayout();}
            Material *          GetMaterial         (){return mat_inst->GetMaterial();}
            MaterialInstance *  GetMaterialInstance (){return mat_inst;}
            Primitive *         GetPrimitive        (){return primitive;}
    const   AABB &              GetBoundingBox      ()const{return primitive->GetBoundingBox();}

    const   VertexInputData *   GetVertexInputData  ()const{return vertex_input;}

            MaterialParameters *GetMP               (const DescriptorSetType &type){return mat_inst->GetMP(type);}

public: //instance support

    virtual const uint32_t      GetInstanceCount    ()const{return 1;}
};//class Renderable

Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDERABLE_INCLUDE
