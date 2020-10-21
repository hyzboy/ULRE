#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKRenderableInstance.h>

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

        void RenderList::Render(SceneNode *node,vulkan::RenderableInstance *ri)
        {
            if(last_pipeline!=ri->GetPipeline())
            {
                last_pipeline=ri->GetPipeline();

                cmd_buf->BindPipeline(last_pipeline);

                last_mat_inst=nullptr;
            }

            if(last_mat_inst!=ri->GetMaterialInstance())
            {
                last_mat_inst=ri->GetMaterialInstance();

                cmd_buf->BindDescriptorSets(last_mat_inst->GetDescriptorSets());
            }

            if(last_pc!=node->GetPushConstant())
            {
                last_pc=node->GetPushConstant();

                cmd_buf->PushConstants(last_pc,sizeof(vulkan::PushConstant));
            }

            //更新fin_mvp

            if(ri!=last_ri)
            {
                cmd_buf->BindVAB(ri);

                last_ri=ri;
            }

            const vulkan::IndexBuffer *ib=ri->GetIndexBuffer();

            if(ib)
            {
                cmd_buf->DrawIndexed(ib->GetCount());
            }
            else
            {
                cmd_buf->Draw(ri->GetDrawCount());
            }
        }

        void RenderList::Render(SceneNode *node,List<vulkan::RenderableInstance *> &ri_list)
        {
            const int count=ri_list.GetCount();
            vulkan::RenderableInstance **ri=ri_list.GetData();

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
            last_ri=nullptr;
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
