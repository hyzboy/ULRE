#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>
#include<hgl/graph/VertexBuffer.h>
#include<hgl/graph/RenderableNode.h>

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

        void RenderList::SetCamera(const Camera &cam)
        {
            camera=cam;

            MakeCameraMatrix(   &ubo_matrix.projection,
                                &ubo_matrix.modelview,
                                &camera);

            ubo_matrix.mvp      =ubo_matrix.projection*ubo_matrix.modelview;
            ubo_matrix.normal   =ubo_matrix.modelview.Float3x3Part();             //法线矩阵为3x3

            CameraToFrustum(&frustum,
                            &camera);
        }

        void RenderList::Render(RenderableNode *ri,const Matrix4f &fin_mvp)
        {
            if(last_pipeline!=ri->GetPipeline())
            {
                cmd_buf->Bind(ri->GetPipeline());

                last_pipeline=ri->GetPipeline();

                cmd_buf->Bind(ri->GetDescriptorSets());
            }
            else
            {
                if(last_desc_sets!=ri->GetDescriptorSets())
                {
                    cmd_buf->Bind(ri->GetDescriptorSets());

                    last_desc_sets=ri->GetDescriptorSets();
                }
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

        bool RenderList::Render()
        {
            if(!cmd_buf)
                return(false);

            last_pipeline=nullptr;
            last_desc_sets=nullptr;
            last_renderable=nullptr;

            const int count=renderable_node_list.GetCount();
            RenderableNode **node=renderable_node_list.GetData();

            for(int i=0;i<count;i++)
            {
                const Matrix4f fin_mv=ubo_matrix.modelview*(*node)->GetLocalToWorldMatrix();

                Render(*node,ubo_matrix.projection*fin_mv);

                node++;
            }

            return(true);
        }
    }//namespace graph
}//namespace hgl
