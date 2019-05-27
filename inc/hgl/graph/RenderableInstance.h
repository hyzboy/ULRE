#ifndef HGL_GRAPH_RENDERABLE_INSTANCE_INCLUDE
#define HGL_GRAPH_RENDERABLE_INSTANCE_INCLUDE

#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKPipeline.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 可渲染对象节点
        */
        class RenderableInstance
        {
            vulkan::Pipeline *        pipeline;
            vulkan::DescriptorSets *  desc_sets;
            vulkan::Renderable *      render_obj;

        public:

            RenderableInstance(vulkan::Pipeline *p,vulkan::DescriptorSets *ds,vulkan::Renderable *r):pipeline(p),desc_sets(ds),render_obj(r){}
            virtual ~RenderableInstance()
            {
                //需要在这里添加删除pipeline/desc_sets/render_obj引用计数的代码
            }

            vulkan::Pipeline *      GetPipeline         (){return pipeline;}
            vulkan::DescriptorSets *GetDescriptorSets   (){return desc_sets;}
            vulkan::Renderable *    GetRenderable       (){return render_obj;}
            const AABB &            GetBoundingBox      ()const{return render_obj->GetBoundingBox();}

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
        };//class RenderableNode
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDERABLE_INSTANCE_INCLUDE
