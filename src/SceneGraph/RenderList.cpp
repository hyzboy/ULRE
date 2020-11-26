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

            return( length_squared(obj_one->GetCenter(),cam->pos)-
                    length_squared(obj_two->GetCenter(),cam->pos));
        }

        //bool FrustumClipFilter(const SceneNode *node,void *fc)
        //{
        //    if(!node||!fc)return(false);

        //    return (((Frustum *)fc)->BoxIn(node->GetWorldBoundingBox())!=Frustum::OUTSIDE);
        //}

        RenderList::RenderList()
        {
            cmd_buf         =nullptr;

            last_pipeline   =nullptr;
            last_mat_inst   =nullptr;
            last_ri         =nullptr;
        }

        void RenderList::Begin()
        {
            LocalToWorld.Clear();
            scene_node_list.ClearData();

            last_pipeline   =nullptr;
            last_mat_inst   =nullptr;
            last_ri         =nullptr;
        }

        void RenderList::Add(SceneNode *node)
        {
            if(!node)return;

            LocalToWorld.Add(node->GetLocalToWorldMatrix());
            
            scene_node_list.Add(node);
        }

        void RenderList::End()
        {
        }

        void RenderList::Render(SceneNode *node,RenderableInstance *ri)
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

            //更新fin_mvp

            if(ri!=last_ri)
            {
                cmd_buf->BindVAB(ri);

                last_ri=ri;
            }

            const IndexBuffer *ib=ri->GetIndexBuffer();

            if(ib)
            {
                cmd_buf->DrawIndexed(ib->GetCount());
            }
            else
            {
                cmd_buf->Draw(ri->GetDrawCount());
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

        bool RenderList::Render(RenderCmdBuffer *cb) 
        {
            if(!cb)
                return(false);

            cmd_buf=cb;

            last_pipeline=nullptr;
            last_mat_inst=nullptr;
            last_ri=nullptr;

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
