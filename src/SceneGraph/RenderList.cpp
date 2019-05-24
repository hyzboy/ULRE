#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>
#include<hgl/graph/VertexBuffer.h>
#include<hgl/math/Math.h>

#include<hgl/graph/vulkan/VKRenderableInstance.h>

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

            MakeCameraMatrix(   &projection_matrix,
                                &modelview_matrix,
                                &camera);

            mvp_matrix=projection_matrix*modelview_matrix;

            CameraToFrustum(&frustum,
                            &camera);
        }

        void RenderList::Render(vulkan::RenderableInstance *ri,const Matrix4f &fin_mvp)
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

            int count=SceneNodeList.GetCount();
            const SceneNode **node=SceneNodeList.GetData();

            for(int i=0;i<count;i++)
            {
                const Matrix4f fin_mv=modelview_matrix*(*node)->GetLocalToWorldMatrix();

                int sn=(*node)->RenderableList.GetCount();
                RenderableInstance **p=(*node)->RenderableList.GetData();

                for(int j=0;j<sn;j++)
                {
                    Render(*p,projection_matrix*fin_mv);

                    p++;
                }

                node++;
            }

            return(true);
        }
    }//namespace graph
}//namespace hgl
