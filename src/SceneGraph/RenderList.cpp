#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKRenderableInstance.h>

namespace hgl
{
    namespace graph
    {
        constexpr size_t MVPMatrixBytes=sizeof(MVPMatrix);

        //bool FrustumClipFilter(const SceneNode *node,void *fc)
        //{
        //    if(!node||!fc)return(false);

        //    return (((Frustum *)fc)->BoxIn(node->GetWorldBoundingBox())!=Frustum::OUTSIDE);
        //}

        RenderList::RenderList(GPUDevice *gd)
        {
            device          =gd;
            cmd_buf         =nullptr;

            last_pipeline   =nullptr;
            last_ri         =nullptr;

            mvp_array       =new MVPArrayBuffer(gd,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        }

        RenderList::~RenderList()
        {
            delete mvp_array;
        }

        void RenderList::Begin()
        {
            scene_node_list.ClearData();

            mvp_array->Clear();
            mvp_offset.ClearData();

            last_pipeline   =nullptr;
            last_ri         =nullptr;
        }

        void RenderList::Add(SceneNode *node)
        {
            if(!node)return;

            scene_node_list.Add(node);
        }

        void RenderList::End(CameraInfo *camera_info)
        {
            //清0计数器
            uint32_t mvp_count=0;       //local to world矩阵总数量

            //统计Render
            const uint32_t count=scene_node_list.GetCount();

            mvp_array->Alloc(count);
            mvp_array->Clear();

            mvp_offset.PreMalloc(count);
            mvp_offset.ClearData();

            MVPMatrix *mp=mvp_array->Map(0,count);
            uint32_t *op=mvp_offset.GetData();

            for(SceneNode *node:scene_node_list)
            {
                const Matrix4f &l2w=node->GetLocalToWorldMatrix();

                if(!l2w.IsIdentity())
                {
                    mp->Set(l2w,camera_info->vp);
                    ++mp;

                    *op=mvp_count*MVPMatrixBytes;
                    ++mvp_count;
                }
                else
                {
                    *op=mvp_count*MVPMatrixBytes;
                }

                ++op;
            }
        }

        void RenderList::Render(SceneNode *node,RenderableInstance *ri)
        {
            if(last_pipeline!=ri->GetPipeline())
            {
                last_pipeline=ri->GetPipeline();

                cmd_buf->BindPipeline(last_pipeline);
            }

            {
                MaterialInstance *mi=ri->GetMaterialInstance();

                cmd_buf->BindDescriptorSets(mi->GetDescriptorSets());
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
