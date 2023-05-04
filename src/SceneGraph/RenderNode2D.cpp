#include<hgl/graph/RenderNode2D.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/util/sort/Sort.h>

/**
* 
* 理论上讲，我们需要按以下顺序排序
*
*   for(material)
*       for(pipeline)
*           for(material_instance)
*               for(vbo)
*/

template<> 
int Comparator<hgl::graph::RenderNode2D>::compare(const hgl::graph::RenderNode2D &obj_one,const hgl::graph::RenderNode2D &obj_two) const
{
    int off;

    hgl::graph::Renderable *ri_one=obj_one.ri;
    hgl::graph::Renderable *ri_two=obj_two.ri;

    //比较管线
    {
        off=ri_one->GetPipeline()
           -ri_two->GetPipeline();

        if(off)
            return off;
    }

    //比较材质实例
    {
        off=ri_one->GetMaterialInstance()
           -ri_two->GetMaterialInstance();

        if(off)
            return off;
    }

    //比较模型
    {
        off=ri_one->GetPrimitive()
           -ri_two->GetPrimitive();

        if(off)
            return off;
    }

    return 0;
}

namespace hgl
{
    namespace graph
    {
        /*
         * 2D渲染节点额外提供的VBO数据
         */
        struct RenderNode2DExtraBuffer
        {
            uint count;

            VBO *local_to_world[3];

        public:

            RenderNode2DExtraBuffer()
            {
                hgl_zero(*this);
            }

            ~RenderNode2DExtraBuffer()
            {
                Clear();
            }

            void Clear()
            {
                SAFE_CLEAR(local_to_world[0])
                SAFE_CLEAR(local_to_world[1])
                SAFE_CLEAR(local_to_world[2])
                count=0;
            }

            void Alloc(GPUDevice *dev,const uint c)
            {
                Clear();
                count=power_to_2(c);

                local_to_world[0]=dev->CreateVBO(VF_V4F,count);
                local_to_world[1]=dev->CreateVBO(VF_V4F,count);
                local_to_world[2]=dev->CreateVBO(VF_V4F,count);
            }

            void WriteData(RenderNode2D *render_node,const uint count)
            {
                RenderNode2D *rn;
                glm::vec4 *tp;

                for(uint col=0;col<3;col++)
                {
                    tp=(glm::vec4 *)(local_to_world[col]->Map());

                    rn=render_node;

                    for(uint i=0;i<count;i++)
                    {
                        *tp=rn->local_to_world[col];
                        ++tp;
                        ++rn;
                    }

                    local_to_world[col]->Unmap();
                }
            }
        };//struct RenderNode2DExtraBuffer

        MaterialRenderList2D::MaterialRenderList2D(GPUDevice *d,RenderCmdBuffer *rcb,Material *m)
        {
            device=d;
            cmd_buf=rcb;
            mtl=m;
            extra_buffer=nullptr;

            const VertexInput *vi=mtl->GetVertexInput();

            binding_count=vi->GetCount();
            buffer_list=new VkBuffer[binding_count];
            buffer_offset=new VkDeviceSize[binding_count];                        
        }

        MaterialRenderList2D::~MaterialRenderList2D()
        {
            delete[] buffer_offset;
            delete[] buffer_list;

            SAFE_CLEAR(extra_buffer)
        }

        void MaterialRenderList2D::Add(Renderable *ri,const Matrix3x4f &mat)
        {
            RenderNode2D rn;

            rn.local_to_world=mat;
            rn.ri=ri;

            rn_list.Add(rn);
        }

        void MaterialRenderList2D::End()
        {
            //排序
            {
                Comparator<hgl::graph::RenderNode2D> rnc;

                Sort(rn_list,&rnc);
            }

            //写入LocalToWorld数据
            {
                uint count=rn_list.GetCount();

                if(count<=0)return;

                if(!extra_buffer)
                    extra_buffer=new RenderNode2DExtraBuffer;

                if(extra_buffer->count<count)
                    extra_buffer->Alloc(device,count);

                //写入数据
                extra_buffer->WriteData(rn_list.GetData(),count);
            }
        }

        void MaterialRenderList2D::Render(const uint index,Renderable *ri)
        {
            if(last_mi!=ri->GetMaterialInstance())
            {
                

                last_pipeline=nullptr;
                last_primitive=nullptr;
            }

            if(last_pipeline!=ri->GetPipeline())
            {
                last_pipeline=ri->GetPipeline();

                cmd_buf->BindPipeline(last_pipeline);

                last_primitive=nullptr;
            }

            if(last_primitive!=ri->GetPrimitive())
            {
                //把之前的一次性画了

                last_primitive=ri->GetPrimitive();

                first_index=index;
            }
        }

        void MaterialRenderList2D::Render()
        {
            last_mi=nullptr;
            last_pipeline=nullptr;
            last_primitive=nullptr;
            first_index=0;

            const uint count=rn_list.GetCount();

            if(count<=0)return;

            RenderNode2D *rn=rn_list.GetData();

            for(uint i=0;i<count;i++)
            {
                Render(i,rn->ri);
                ++rn;
            }
        }
    }//namespace graph
}//namespace hgl
