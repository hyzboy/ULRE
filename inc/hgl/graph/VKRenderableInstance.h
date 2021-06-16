#ifndef HGL_GRAPH_RENDERABLE_INSTANCE_INCLUDE
#define HGL_GRAPH_RENDERABLE_INSTANCE_INCLUDE

#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
VK_NAMESPACE_BEGIN
/**
* 可渲染对象实例<br>
* RenderList会统一管理Shader中的LocalToWorld数据，使用DynamicUBO/DynamicSSBO实现。
*/
class RenderableInstance                                                        ///可渲染对象实例
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    Renderable *        render_obj;

    MaterialParameters *mp_r;

    uint32_t            buffer_count;
    VkBuffer *          buffer_list;
    VkDeviceSize *      buffer_size;

    uint32_t            buffer_hash;

private:

    friend RenderableInstance *CreateRenderableInstance(Renderable *,MaterialInstance *,Pipeline *);

    RenderableInstance(Renderable *,MaterialInstance *,Pipeline *,const uint32_t,VkBuffer *,VkDeviceSize *);

public:

    virtual ~RenderableInstance();

            Pipeline *          GetPipeline         (){return pipeline;}
            VkPipelineLayout    GetPipelineLayout   (){return mat_inst->GetMaterial()->GetPipelineLayout();}
            MaterialInstance *  GetMaterialInstance (){return mat_inst;}
            Renderable *        GetRenderable       (){return render_obj;}
    const   AABB &              GetBoundingBox      ()const{return render_obj->GetBoundingBox();}

    const   uint32_t            GetBufferCount      ()const{return buffer_count;}
            VkBuffer *          GetBuffer           ()const{return buffer_list;}
            VkDeviceSize *      GetBufferSize       ()const{return buffer_size;}
            IndexBuffer *       GetIndexBuffer      ()const{return render_obj->GetIndexBuffer();}
    const   uint32_t            GetIndexBufferOffset()const{return render_obj->GetIndexBufferOffset();}
    const   uint32_t            GetDrawCount        ()const{return render_obj->GetDrawCount();}

    const   uint32_t            GetBufferHash       ()const{return buffer_hash;}

            MaterialParameters *GetMP               (const DescriptorSetsType &type);
};//class RenderableInstance

RenderableInstance *CreateRenderableInstance(Renderable *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDERABLE_INSTANCE_INCLUDE
