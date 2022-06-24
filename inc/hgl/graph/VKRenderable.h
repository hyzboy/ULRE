#ifndef HGL_GRAPH_RENDERABLE_INCLUDE
#define HGL_GRAPH_RENDERABLE_INCLUDE

#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
VK_NAMESPACE_BEGIN
/**
* 可渲染对象<br>
* RenderList会统一管理Shader中的LocalToWorld数据，使用DynamicUBO/DynamicSSBO实现。
*/
class Renderable                                                                ///可渲染对象实例
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    Primitive *         primitive;

    uint32_t            buffer_count;
    VkBuffer *          buffer_list;
    VkDeviceSize *      buffer_size;

    uint32_t            buffer_hash;

private:

    friend Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);

    Renderable(Primitive *,MaterialInstance *,Pipeline *,const uint32_t,VkBuffer *,VkDeviceSize *);

public:

    virtual ~Renderable();

            void                UpdatePipeline      (Pipeline *p){pipeline=p;}

            Pipeline *          GetPipeline         (){return pipeline;}
            VkPipelineLayout    GetPipelineLayout   (){return mat_inst->GetMaterial()->GetPipelineLayout();}
            Material *          GetMaterial         (){return mat_inst->GetMaterial();}
            MaterialInstance *  GetMaterialInstance (){return mat_inst;}
            Primitive *         GetPrimitive        (){return primitive;}
    const   AABB &              GetBoundingBox      ()const{return primitive->GetBoundingBox();}

    const   uint32_t            GetBufferCount      ()const{return buffer_count;}
            VkBuffer *          GetBuffer           ()const{return buffer_list;}
            VkDeviceSize *      GetBufferSize       ()const{return buffer_size;}
            IndexBuffer *       GetIndexBuffer      ()const{return primitive->GetIndexBuffer();}
    const   uint32_t            GetIndexBufferOffset()const{return primitive->GetIndexBufferOffset();}
    const   uint32_t            GetDrawCount        ()const{return primitive->GetDrawCount();}

    const   uint32_t            GetBufferHash       ()const{return buffer_hash;}

            MaterialParameters *GetMP               (const DescriptorSetsType &type){return mat_inst->GetMP(type);}

public: //instance support

    virtual const uint32_t      GetInstanceCount    ()const{return 1;}
};//class Renderable

Renderable *CreateRenderable(Primitive *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDERABLE_INCLUDE
