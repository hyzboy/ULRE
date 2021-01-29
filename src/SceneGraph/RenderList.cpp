#include<hgl/graph/RenderList.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKRenderableInstance.h>

namespace hgl
{
    namespace graph
    {
        /** 
         * MVP矩阵
         */
        struct MVPMatrix
        {
            Matrix4f l2w;                   ///< Local to World
            Matrix4f inverse_l2w;

            Matrix4f mvp;                   ///< projection * view * local_to_world
            Matrix4f inverse_mvp;

        public:

            void Set(const Matrix4f &w,const Matrix4f &vp)
            {
                l2w=w;
                inverse_l2w=l2w.Inverted();

                mvp=vp*l2w;
                inverse_mvp=mvp.Inverted();
            }
        };//struct MVPMatrix

        constexpr size_t MVPMatrixBytes=sizeof(MVPMatrix);


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

        RenderList::RenderList(GPUDevice *gd)
        {
            device          =gd;
            cmd_buf         =nullptr;

            last_pipeline   =nullptr;
            last_ri         =nullptr;

            mvp_array=new MVPArray;
        }

        RenderList::~RenderList()
        {
            delete mvp_array;
        }

        void RenderList::Begin()
        {
            scene_node_list.ClearData();

            last_pipeline   =nullptr;
            last_ri         =nullptr;
        }

        void RenderList::Add(SceneNode *node)
        {
            if(!node)return;

            scene_node_list.Add(node);
        }

        void RenderList::End(CameraMatrix *camera_matrix)
        {
            //清0计数器
            uint32_t mvp_count=0;       //local to world矩阵总数量

            //统计Render
            const uint32_t count=scene_node_list.GetCount();

            mvp_array->Init(count);

            MVPMatrix *mp=mvp_array->items.GetData();
            uint32_t *op=mvp_array->offset.GetData();

            for(SceneNode *node:scene_node_list)
            {
                const Matrix4f &l2w=node->GetLocalToWorldMatrix();

                if(!l2w.IsIdentity())
                {
                    mp->Set(l2w,camera_matrix->vp);
                    ++mp;

                    *op=(mvp_count)*MVPMatrixBytes;
                    ++mvp_count;
                }
                else
                {
                    *op=mvp_count;
                }

                ++op;
            }

            if(mvp_array->buffer)
            {
                if(mvp_array->alloc_count<mvp_count)
                {
                    mvp_array->buffer->Unmap();
                    delete mvp_array->buffer;
                    mvp_array->buffer=nullptr;
//                    mvp_array->buffer_address=nullptr;     //下面一定会重写，所以这一行没必要操作
                }
            }

            if(!mvp_array->buffer)
            {
                mvp_array->alloc_count=power_to_2(mvp_count);
                mvp_array->buffer=device->CreateUBO(mvp_array->alloc_count*MVPMatrixBytes);
                mvp_array->buffer_address=(MVPMatrix *)(mvp_array->buffer->Map());
            }

            hgl_cpy(mvp_array->buffer_address,mvp_array->items.GetData(),mvp_count);
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
