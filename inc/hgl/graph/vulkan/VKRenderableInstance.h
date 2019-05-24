#ifndef HGL_GRAPH_VULKAN_RENDERABLE_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_RENDERABLE_INSTANCE_INCLUDE

#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKPipeline.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>
VK_NAMESPACE_BEGIN
/**
* 可渲染对象实例
*/
class RenderableInstance
{
    Pipeline *        pipeline;
    DescriptorSets *  desc_sets;
    Renderable *      render_obj;

public:

    RenderableInstance(Pipeline *p,DescriptorSets *ds,Renderable *r):pipeline(p),desc_sets(ds),render_obj(r){}
    virtual ~RenderableInstance()
    {
    }

    Pipeline *      GetPipeline         (){return pipeline;}
    DescriptorSets *GetDescriptorSets   (){return desc_sets;}
    Renderable *    GetRenderable       (){return render_obj;}

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

        if(desc_sets!=ri->desc_sets)
            return desc_sets-ri->desc_sets;

        return render_obj-ri->render_obj;
    }

    CompOperator(const RenderableInstance *,Comp)
};//class RenderableInstance
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDERABLE_INSTANCE_INCLUDE
