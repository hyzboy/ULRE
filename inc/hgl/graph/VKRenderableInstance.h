#ifndef HGL_GRAPH_RENDERABLE_INSTANCE_INCLUDE
#define HGL_GRAPH_RENDERABLE_INSTANCE_INCLUDE

#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKMaterialInstance.h>
VK_NAMESPACE_BEGIN
/**
* 可渲染对象节点
*/
class RenderableInstance
{
    Pipeline *          pipeline;
    MaterialInstance *  mat_inst;
    Renderable *        render_obj;

    uint32_t            buffer_count;
    VkBuffer *          buffer_list;
    VkDeviceSize *      buffer_size;

private:

    friend RenderableInstance *CreateRenderableInstance(Renderable *,MaterialInstance *,Pipeline *);

    RenderableInstance(Renderable *,MaterialInstance *,Pipeline *,const uint32_t,VkBuffer *,VkDeviceSize *);

public:

    virtual ~RenderableInstance();

            Pipeline *          GetPipeline         (){return pipeline;}
            MaterialInstance *  GetMaterialInstance (){return mat_inst;}
            Renderable *        GetRenderable       (){return render_obj;}
    const   AABB &              GetBoundingBox      ()const{return render_obj->GetBoundingBox();}

    const   uint32_t            GetBufferCount      ()const{return buffer_count;}
            VkBuffer *          GetBuffer           ()const{return buffer_list;}
            VkDeviceSize *      GetBufferSize       ()const{return buffer_size;}
            IndexBuffer *       GetIndexBuffer      ()const{return render_obj->GetIndexBuffer();}
    const   uint32_t            GetIndexBufferOffset()const{return render_obj->GetIndexBufferOffset();}
    const   uint32_t            GetDrawCount        ()const{return render_obj->GetDrawCount();}

            DescriptorSets *    GetDescriptorSets   ()const{return mat_inst->GetDescriptorSets();}

public:

    const int Comp(const RenderableInstance *ri)const
    {
        //绘制顺序：

        //  ARM Mali GPU :   不透明、AlphaTest、半透明
        //  Adreno/NV/AMD:   AlphaTest、不透明、半透明
        //  PowerVR:         同Adreno/NV/AMD，但不透明部分可以不排序

        if(pipeline->IsAlphaBlend())
        {
            if(!ri->pipeline->IsAlphaBlend())
                return 1;
        }
        else
        {
            if(ri->pipeline->IsAlphaBlend())
                return -1;
        }

        if(pipeline->IsAlphaTest())
        {
            if(!ri->pipeline->IsAlphaTest())
                return 1;
        }
        else
        {
            if(ri->pipeline->IsAlphaTest())
                return -1;
        }

        if(pipeline!=ri->pipeline)
            return pipeline-ri->pipeline;

        if(mat_inst!=ri->mat_inst)
            return int64(mat_inst)-int64(ri->mat_inst);

        return render_obj-ri->render_obj;
    }

    CompOperator(const RenderableInstance *,Comp)
};//class RenderableInstance

RenderableInstance *CreateRenderableInstance(Renderable *,MaterialInstance *,Pipeline *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDERABLE_INSTANCE_INCLUDE
