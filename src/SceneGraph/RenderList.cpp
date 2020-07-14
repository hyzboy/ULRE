#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>
#include<hgl/graph/VertexAttribBuffer.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/vulkan/VKMaterialInstance.h>

namespace hgl
{
    namespace graph
    {
        float CameraLengthComp(Camera *cam,SceneNode *obj_one,SceneNode *obj_two)
        {
            if(!cam||!obj_one||!obj_two)
                return(0);

            return( length_squared(obj_one->GetCenter(),cam->eye)-
                    length_squared(obj_two->GetCenter(),cam->eye));
        }

        //bool FrustumClipFilter(const SceneNode *node,void *fc)
        //{
        //    if(!node||!fc)return(false);

        //    return (((Frustum *)fc)->BoxIn(node->GetWorldBoundingBox())!=Frustum::OUTSIDE);
        //}

        void RenderList::Render(SceneNode *node,RenderableInstance *ri)
        {
            if(last_pipeline!=ri->GetPipeline())
            {
                last_pipeline=ri->GetPipeline();

                cmd_buf->Bind(last_pipeline);

                last_mat_inst=nullptr;
            }

            if(last_mat_inst!=ri->GetMaterialInstance())
            {
                last_mat_inst=ri->GetMaterialInstance();

                cmd_buf->Bind(last_mat_inst->GetDescriptorSets());
            }

            if(last_pc!=node->GetPushConstant())
            {
                last_pc=node->GetPushConstant();

                cmd_buf->PushConstants(last_pc,sizeof(vulkan::PushConstant));
            }

            //更新fin_mvp

            vulkan::Renderable *obj=ri->GetRenderable();

            if(obj!=last_renderable)
            {
                cmd_buf->Bind(obj);

                last_renderable=obj;
            }

            const vulkan::IndexBuffer *ib=obj->GetIndexBuffer();

            if(ib)
            {
                cmd_buf->DrawIndexed(ib->GetCount());
            }
            else
            {
                cmd_buf->Draw(obj->GetDrawCount());
            }
        }

        void RenderList::Render(SceneNode *node,List<RenderableInstance *> &ri_list)
        {
            const int count=ri_list.GetCount();
            RenderableInstance **ri=ri_list.GetData();

            for(int i=0;i<count;i++)
            {
                Render(node,*ri);
                ++ri;
            }
        }

        bool RenderList::Render(vulkan::CommandBuffer *cb) 
        {
            if(!cb)
                return(false);

            cmd_buf=cb;

            last_pipeline=nullptr;
            last_mat_inst=nullptr;
            last_renderable=nullptr;
            last_pc=nullptr;

            const int count=scene_node_list.GetCount();
            SceneNode **node=scene_node_list.GetData();

            for(int i=0;i<count;i++)
            {
                Render(*node,(*node)->renderable_instances);
                ++node;
            }

            return(true);
        }
    }//namespace graph
}//namespace hgl
