#ifndef HGL_GRAPH_RENDERABLE_NODE_INCLUDE
#define HGL_GRAPH_RENDERABLE_NODE_INCLUDE

#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKPipeline.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>
#include<hgl/graph/SceneNode.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 可渲染对象节点
        */
        class RenderableNode:public SceneNode
        {
            vulkan::Pipeline *        pipeline;
            vulkan::DescriptorSets *  desc_sets;
            vulkan::Renderable *      render_obj;

        public:

            RenderableNode(vulkan::Pipeline *p,vulkan::DescriptorSets *ds,vulkan::Renderable *r):pipeline(p),desc_sets(ds),render_obj(r){}
            virtual ~RenderableNode()
            {
                //需要在这里添加删除pipeline/desc_sets/render_obj引用计数的代码
            }

            vulkan::Pipeline *      GetPipeline         (){return pipeline;}
            vulkan::DescriptorSets *GetDescriptorSets   (){return desc_sets;}
            vulkan::Renderable *    GetRenderable       (){return render_obj;}
            const AABB &            GetBoundingBox      ()const{return render_obj->GetBoundingBox();}

            const bool              CanRenderable       ()const override{return true;}              ///<是否可以渲染

            const int Comp(const RenderableNode *ri)const
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

            CompOperator(const RenderableNode *,Comp)
        };//class RenderableNode
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDERABLE_NODE_INCLUDE
