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

            VBO *l2w_vbo[3];
            VkBuffer l2w_buffer[3];

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
                SAFE_CLEAR(l2w_vbo[0])
                SAFE_CLEAR(l2w_vbo[1])
                SAFE_CLEAR(l2w_vbo[2])
                count=0;
            }

            void Alloc(GPUDevice *dev,const uint c)
            {
                Clear();
                count=power_to_2(c);

                for(uint i=0;i<3;i++)
                {
                    l2w_vbo[i]=dev->CreateVBO(VF_V4F,count);
                    l2w_buffer[i]=l2w_vbo[i]->GetBuffer();
                }
            }

            void WriteData(RenderNode2D *render_node,const uint count)
            {
                RenderNode2D *rn;
                glm::vec4 *tp;

                for(uint col=0;col<3;col++)
                {
                    tp=(glm::vec4 *)(l2w_vbo[col]->Map());

                    rn=render_node;

                    for(uint i=0;i<count;i++)
                    {
                        *tp=rn->local_to_world[col];
                        ++tp;
                        ++rn;
                    }

                    l2w_vbo[col]->Unmap();
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

        void MaterialRenderList2D::RenderItem::Set(Renderable *ri)
        {
            pipeline    =ri->GetPipeline();
            mi          =ri->GetMaterialInstance();
            vid         =ri->GetVertexInputData();
        }

        void MaterialRenderList2D::Stat()
        {
            const uint count=rn_list.GetCount();
            RenderNode2D *rn=rn_list.GetData();

            ri_list.ClearData();
            ri_list.PreMalloc(count);

            RenderItem *ri=ri_list.GetData();
            
            ri_count=1;

            ri->first=0;
            ri->count=1;
            ri->Set(rn->ri);

            last_pipeline   =ri->pipeline;
            last_mi         =ri->mi;
            last_vid        =ri->vid;

            ++rn;

            for(uint i=1;i<count;i++)
            {   
                if(last_pipeline==rn->ri->GetPipeline())
                    if(last_mi==rn->ri->GetMaterialInstance())
                        if(last_vid==rn->ri->GetVertexInputData())
                        {
                            ++ri->count;
                            ++rn;
                            continue;
                        }

                ++ri_count;

                ++ri;
                ri->first=i;
                ri->count=1;
                ri->Set(rn->ri);

                last_pipeline   =ri->pipeline;
                last_mi         =ri->mi;
                last_vid        =ri->vid;

                ++rn;
            }
        }

        void MaterialRenderList2D::Bind(MaterialInstance *mi)
        {
        }

        bool MaterialRenderList2D::Bind(const VertexInputData *vid,const uint first)
        {
            //binding号都是在VertexInput::CreateVIL时连续紧密排列生成的，所以bind时first_binding写0就行了。

            const VIL *vil=last_mi->GetVIL();

            if(vil->GetCount(VertexInputGroup::Basic)!=vid->binding_count)
                return(false);                                                  //这里基本不太可能，因为CreateRenderable时就会检查值是否一样

            uint count=0;

            //Basic组，它所有的VBO信息均来自于Primitive，由vid参数传递进来
            {
                hgl_cpy(buffer_list,vid->buffer_list,vid->binding_count);
                hgl_cpy(buffer_offset,vid->buffer_offset,vid->binding_count);

                if(binding_count==vid->binding_count)
                    return(true);

                count=vid->binding_count;
            }

            //Bone组，暂未支持
            {
                const uint bone_binding_count=vil->GetCount(VertexInputGroup::Bone);

                if(bone_binding_count>0)                                        //有骨骼矩阵信息
                {
                    if(bone_binding_count!=2)                                   //只有BoneID/BondWeight，，，不是2的话根本就不对
                        return(false);

                    count+=bone_binding_count;
                }
            }

            //LocalToWorld组，由RenderList合成
            {
                const uint l2w_binding_count=vil->GetCount(VertexInputGroup::LocalToWorld);

                if(l2w_binding_count>0)                                         //有变换矩阵信息
                {
                    if(l2w_binding_count!=3)                                    //2D的l2w使用mat3x4f，应该只有3个
                        return(false);

                    hgl_cpy(buffer_list+count,extra_buffer->l2w_buffer,3);

                    for(uint i=0;i<3;i++)
                        buffer_offset[count+i]=first*16;                        //mat3x4f每列都是rgba32f，自然是16字节

                    count+=l2w_binding_count;
                }
            }

            //
            if(count!=binding_count)
            {
                //还有没支持的绑定组？？？？

                return(false);
            }

            return(true);
        }

        void MaterialRenderList2D::Render(RenderItem *ri)
        {
            if(last_pipeline!=ri->pipeline)
            {
                cmd_buf->BindPipeline(ri->pipeline);
                last_pipeline=ri->pipeline;

                last_mi=nullptr;
                last_vid=nullptr;

                //这里未来尝试换pipeline同时不换mi/primitive是否需要重新绑定primitive
            }

            if(last_mi!=ri->mi)
            {
                Bind(ri->mi);
                last_mi=ri->mi;

                last_vid=nullptr;
            }

            if(!last_vid->Comp(ri->vid))
            {
                Bind(ri->vid,ri->first);
                last_vid=ri->vid;
            }

            const IndexBufferData *ibd=last_primitive->GetIndexBuffer();

            if(ib)
                cmd_buf->DrawIndexed(ib->GetCount(),ri->count);
            else
                cmd_buf->Draw(last_primitive->GetDrawCount(),ri->count);
        }

        void MaterialRenderList2D::Render()
        {
            const uint count=rn_list.GetCount();

            if(count<=0)return;

            Stat();

            if(ri_count<=0)return;

            RenderItem *ri=ri_list.GetData();

            last_pipeline   =nullptr;
            last_mi         =nullptr;
            last_vid        =nullptr;

            for(uint i=0;i<ri_count;i++)
            {
                Render(ri);
                ++ri;
            }
        }
    }//namespace graph
}//namespace hgl
