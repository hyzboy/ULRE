#include<hgl/graph/MaterialRenderList.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/util/sort/Sort.h>
#include"RenderExtraBuffer.h"

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

VK_NAMESPACE_BEGIN
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

        Sort(rn_list.GetArray(),&rnc);
    }

    const uint node_count=rn_list.GetCount();

    if(node_count<=0)return;

    if(!extra_buffer)
        extra_buffer=new RenderExtraBuffer;

    if(extra_buffer->node_count<node_count)
        extra_buffer->NodeAlloc(device,node_count);

    Stat();

    //写入LocalToWorld数据
    extra_buffer->WriteLocalToWorld(rn_list.GetData(),node_count);

    const uint mi_count=mi_set.GetCount();

    if(mi_count<=0)return;

    //if(extra_buffer->mi_count<mi_count)
    //    extra_buffer->MIAlloc(device,mi_count,);
    //extra_buffer->WriteMaterialInstance(rn_list.GetData(),node_count,mi_set);
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
    ri_list.PreAlloc(count);

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

bool MaterialRenderList::Bind(const VertexInputData *vid,const uint ri_index)
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
                buffer_offset[count+i]=ri_index*16;                        //mat4每列都是rgba32f，自然是16字节

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
VK_NAMESPACE_END
