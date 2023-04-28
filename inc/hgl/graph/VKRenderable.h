#ifndef HGL_GRAPH_RENDERABLE_INCLUDE
#define HGL_GRAPH_RENDERABLE_INCLUDE

#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
VK_NAMESPACE_BEGIN

struct VertexInputData
{
    uint32_t count;

    const char **name_list;
    const VkVertexInputBindingDescription *bind_list;
    const VkVertexInputAttributeDescription *attr_list;
    VkBuffer *buffer_list;
    VkDeviceSize *buffer_offset;

public:

    VertexInputData(const VIL *vil);
    ~VertexInputData();
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

    uint32_t            buffer_hash;

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

    const   VertexInputData *   GetVertexInputData  ()const{return vertex_input_data;}

            IndexBuffer *       GetIndexBuffer      ()const{return primitive->GetIndexBuffer();}
    const   uint32_t            GetIndexBufferOffset()const{return primitive->GetIndexBufferOffset();}
    const   uint32_t            GetDrawCount        ()const{return primitive->GetDrawCount();}

    const   uint32_t            GetBufferHash       ()const{return buffer_hash;}

            MaterialParameters *GetMP               (const DescriptorSetType &type){return mat_inst->GetMP(type);}

public: //instance support

    virtual const uint32_t      GetInstanceCount    ()const{return 1;}
};//class Renderable

Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDERABLE_INCLUDE
