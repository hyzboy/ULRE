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
        constexpr uint32_t L2WItemBytes=sizeof(Matrix4f);               ///<单个L2W数据字节数

        struct L2WArrays
        {
            uint alloc_count;
            uint count;

            List<Matrix4f> items;
            GPUBuffer *buffer;
            Matrix4f *buffer_address;
            List<uint32_t> offset;

        public:

            L2WArrays()
            {
                alloc_count=0;
                count=0;
                buffer=nullptr;
                buffer_address=nullptr;
            }

            ~L2WArrays()
            {
                if(buffer)
                {
                    buffer->Unmap();
                    delete buffer;
                }
            }

            void Init(const uint32_t c)
            {
                count=c;
                items.SetCount(count);
                offset.SetCount(count);
            }
        };//

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

            LocalToWorld=new L2WArrays;
        }

        RenderList::~RenderList()
        {
            delete LocalToWorld;
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

        void RenderList::End()
        {
            //清0计数器
            uint32_t l2w_count=0;       //local to world矩阵总数量

            //统计Render
            const uint32_t count=scene_node_list.GetCount();

            LocalToWorld->Init(count);

            Matrix4f *mp=LocalToWorld->items.GetData();
            uint32_t *op=LocalToWorld->offset.GetData();

            for(SceneNode *node:scene_node_list)
            {
                const Matrix4f &mat=node->GetLocalToWorldMatrix();

                if(!mat.IsIdentity())
                {
                    hgl_cpy(mp,&mat);
                    ++mp;

                    *op=(l2w_count)*L2WItemBytes;
                    ++l2w_count;
                }
                else
                {
                    *op=l2w_count;
                }

                ++op;
            }

            if(LocalToWorld->buffer)
            {
                if(LocalToWorld->alloc_count<l2w_count)
                {
                    LocalToWorld->buffer->Unmap();
                    delete LocalToWorld->buffer;
                    LocalToWorld->buffer=nullptr;
//                    LocalToWorld->buffer_address=nullptr;     //下面一定会重写，所以这一行没必要操作
                }
            }

            if(!LocalToWorld->buffer)
            {
                LocalToWorld->alloc_count=power_to_2(l2w_count);
                LocalToWorld->buffer=device->CreateUBO(LocalToWorld->alloc_count*L2WItemBytes);
                LocalToWorld->buffer_address=(Matrix4f *)(LocalToWorld->buffer->Map());
            }

            hgl_cpy(LocalToWorld->buffer_address,LocalToWorld->items.GetData(),l2w_count);
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
