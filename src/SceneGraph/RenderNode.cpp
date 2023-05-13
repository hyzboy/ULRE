#include<hgl/graph/RenderNode.h>
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
int Comparator<hgl::graph::RenderNode>::compare(const hgl::graph::RenderNode &obj_one,const hgl::graph::RenderNode &obj_two) const
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
    // ubo_range大致分为三档:
    //
    //  16k: Mali-T系列或更早、Mali-G71、nVidia GeForce RTX 3070 Laptop为16k
    // 
    //  64k: 大部分手机与PC均为64k
    // 
    // >64k: Intel 核显与 PowerVR 为128MB，AMD显卡为4GB，可视为随显存无上限。
    // 
    // 我们使用uint8类型在vertex input中保存MaterialInstance ID，表示范围0-255。
    // 所以MaterialInstance结构容量按16k/64k分为两个档次，64字节和256字节

    // 如果一定要使用超过16K/64K硬件限制的容量，有两种办法
    // 一、分多次渲染，使用UBO Offset偏移UBO数据区。
    // 二、使用SSBO，但这样会导致性能下降，所以不推荐使用。

    // 但我们不解决这个问题
    // 我们天然要求将材质实例数据分为两个等级，同时要求一次渲染不能超过256种材质实例。
    // 所以 UBO Range为16k时，实例数据不能超过64字节。UBO Range为64k时，实例数据不能超过256字节。

        /*
         * 渲染节点额外提供的数据
         */
        struct RenderNodeExtraBuffer
        {
            uint node_count;                            ///<渲染节点数量
            uint mi_count;                              ///<材质实例数量

            uint mi_size;                               ///<单个材质实例数量长度
            DeviceBuffer *mi_data_buffer;               ///<材质实例数据UBO/SSBO

            VBO *mi_id;
            VkBuffer mi_id_buffer;

            VBO *bone_id,*bone_weight;
            VkBuffer bone_id_buffer,bone_weight_buffer;

            VBO *l2w_vbo[4];
            VkBuffer l2w_buffer[4];

        public:

            RenderNodeExtraBuffer()
            {
                hgl_zero(*this);
            }

            ~RenderNodeExtraBuffer()
            {
                Clear();
            }

            void Clear()
            {
                SAFE_CLEAR(mi_id)
                SAFE_CLEAR(mi_data_buffer);

                SAFE_CLEAR(bone_id)
                SAFE_CLEAR(bone_weight)

                SAFE_CLEAR(l2w_vbo[0])
                SAFE_CLEAR(l2w_vbo[1])
                SAFE_CLEAR(l2w_vbo[2])
                SAFE_CLEAR(l2w_vbo[3])
                node_count=0;
            }

            void NodeAlloc(GPUDevice *dev,const uint c)
            {
                Clear();
                node_count=power_to_2(c);

                for(uint i=0;i<4;i++)
                {
                    l2w_vbo[i]=dev->CreateVBO(VF_V4F,node_count);
                    l2w_buffer[i]=l2w_vbo[i]->GetBuffer();
                }
            }

            void MIAlloc(GPUDevice *dev,const uint c)
            {
                if(c<=0||mi_size<=0)return;
    
                mi_count=power_to_2(c);

                mi_id=dev->CreateVBO(VF_V1U8,mi_count);
                mi_id_buffer=mi_id->GetBuffer();

                mi_data_buffer=dev->CreateUBO(mi_count*mi_size);
            }

            void WriteData(RenderNode *render_node,const uint count)
            {
                RenderNode *rn;
                glm::vec4 *tp;

                for(uint col=0;col<4;col++)
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
        };//struct RenderNodeExtraBuffer

        MaterialRenderList::MaterialRenderList(GPUDevice *d,Material *m)
        {
            device=d;
            cmd_buf=nullptr;
            mtl=m;
            extra_buffer=nullptr;

            const VertexInput *vi=mtl->GetVertexInput();

            binding_count=vi->GetCount();
            buffer_list=new VkBuffer[binding_count];
            buffer_offset=new VkDeviceSize[binding_count];                        
        }

        MaterialRenderList::~MaterialRenderList()
        {
            delete[] buffer_offset;
            delete[] buffer_list;

            SAFE_CLEAR(extra_buffer)
        }

        void MaterialRenderList::Add(Renderable *ri,const Matrix4f &mat)
        {
            RenderNode rn;

            rn.local_to_world=mat;
            rn.ri=ri;

            rn_list.Add(rn);
        }

        void MaterialRenderList::End()
        {
            //排序
            {
                Comparator<hgl::graph::RenderNode> rnc;

                Sort(rn_list,&rnc);
            }

            //写入LocalToWorld数据
            {
                uint count=rn_list.GetCount();

                if(count<=0)return;

                if(!extra_buffer)
                    extra_buffer=new RenderNodeExtraBuffer;

                if(extra_buffer->node_count<count)
                    extra_buffer->NodeAlloc(device,count);

                //写入数据
                extra_buffer->WriteData(rn_list.GetData(),count);
            }

            Stat();
        }

        void MaterialRenderList::RenderItem::Set(Renderable *ri)
        {
            pipeline    =ri->GetPipeline();
            mi          =ri->GetMaterialInstance();
            vid         =ri->GetVertexInputData();
        }

        void MaterialRenderList::Stat()
        {
            const uint count=rn_list.GetCount();
            RenderNode *rn=rn_list.GetData();

            ri_list.ClearData();
            ri_list.PreMalloc(count);

            mi_set.ClearData();

            RenderItem *ri=ri_list.GetData();
            
            ri_count=1;

            ri->first=0;
            ri->count=1;
            ri->Set(rn->ri);

            last_pipeline   =ri->pipeline;
            last_mi         =ri->mi;
            last_vid        =ri->vid;

            mi_set.Add(last_mi);

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

                if(last_mi!=ri->mi)
                    mi_set.Add(ri->mi);

                last_pipeline   =ri->pipeline;
                last_mi         =ri->mi;
                last_vid        =ri->vid;

                ++rn;
            }
        }

        void MaterialRenderList::Bind(MaterialInstance *mi)
        {
        }

        bool MaterialRenderList::Bind(const VertexInputData *vid,const uint first)
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

                count=vid->binding_count;
            }

            if(count<binding_count) //材质组
            {
                const uint mtl_binding_count=vil->GetCount(VertexInputGroup::MaterialInstanceID);

                if(mtl_binding_count>0)
                {
                    if(mtl_binding_count!=1)                                    //只有MaterialInstanceID
                        return(false);

                    count+=mtl_binding_count;
                }
            }

            if(count<binding_count) //Joint组，暂未支持
            {
                const uint joint_id_binding_count=vil->GetCount(VertexInputGroup::JointID);

                if(joint_id_binding_count>0)                                        //有矩阵信息
                {
                    count+=joint_id_binding_count;

                    if(count<binding_count) //JointWeight组
                    {
                        const uint joing_weight_binding_count=vil->GetCount(VertexInputGroup::JointWeight);

                        if(joing_weight_binding_count!=1)
                        {
                            ++count;
                        }
                        else //JointWieght不为1是个bug，除非未来支持8权重
                        {
                            return(false);
                        }
                    }
                    else //有JointID没有JointWeight? 这是个BUG
                    {
                        return(false);
                    }
                }
            }

            if(count<binding_count)//LocalToWorld组，由RenderList合成
            {
                const uint l2w_binding_count=vil->GetCount(VertexInputGroup::LocalToWorld);

                if(l2w_binding_count>0)                                         //有变换矩阵信息
                {
                    if(l2w_binding_count!=4)
                        return(false);

                    hgl_cpy(buffer_list+count,extra_buffer->l2w_buffer,4);

                    for(uint i=0;i<4;i++)
                        buffer_offset[count+i]=first*16;                        //mat4每列都是rgba32f，自然是16字节

                    count+=l2w_binding_count;
                }
            }

            if(count!=binding_count)
            {
                //还有没支持的绑定组？？？？

                return(false);
            }

            cmd_buf->BindVBO(0,count,buffer_list,buffer_offset);

            return(true);
        }

        void MaterialRenderList::Render(RenderItem *ri)
        {
            if(last_pipeline!=ri->pipeline)
            {
                cmd_buf->BindPipeline(ri->pipeline);
                last_pipeline=ri->pipeline;

                last_mi=nullptr;
                last_vid=nullptr;

                //这里未来尝试换pipeline同时不换mi/primitive是否需要重新绑定mi/primitive
            }

            if(last_mi!=ri->mi)
            {
                Bind(ri->mi);
                last_mi=ri->mi;

                last_vid=nullptr;
            }

            if(!ri->vid->Comp(last_vid))
            {
                Bind(ri->vid,ri->first);
                last_vid=ri->vid;
            }

            const IndexBufferData *ibd=last_vid->index_buffer;

            if(ibd->buffer)
            {
                cmd_buf->BindIBO(ibd);

                cmd_buf->DrawIndexed(ibd->buffer->GetCount(),ri->count);
            }
            else
            {
                cmd_buf->Draw(last_vid->vertex_count,ri->count);
            }
        }

        void MaterialRenderList::Render(RenderCmdBuffer *rcb)
        {
            if(!rcb)return;
            const uint count=rn_list.GetCount();

            if(count<=0)return;

            if(ri_count<=0)return;

            cmd_buf=rcb;

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
