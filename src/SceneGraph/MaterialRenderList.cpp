#include<hgl/graph/MaterialRenderList.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/util/sort/Sort.h>
#include"RenderAssignBuffer.h"

/**
* 
* 理论上讲，我们需要按以下顺序排序
*
*   for(material)
*       for(pipeline)
*           for(material_instance)
*               for(vab)
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
MaterialRenderList::MaterialRenderList(GPUDevice *d,bool l2w,Material *m)
{
    device=d;
    cmd_buf=nullptr;
    material=m;

    if(l2w)
        l2w_buffer=new RenderL2WBuffer(device);
    else
        l2w_buffer=nullptr;

    if(material->HasMI())
        mi_buffer=new RenderMIBuffer(device,material->GetMIDataBytes());
    else
        mi_buffer=nullptr;

    vbo_list=new VBOList(material->GetVertexInput()->GetCount());
}

MaterialRenderList::~MaterialRenderList()
{
    SAFE_CLEAR(vbo_list);
    SAFE_CLEAR(mi_buffer);
    SAFE_CLEAR(l2w_buffer);
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
    Sort(rn_list.GetArray());

    const uint node_count=rn_list.GetCount();

    if(node_count<=0)return;

    Stat();

    if(l2w_buffer)
    {
        l2w_buffer->WriteNode(rn_list.GetData(),node_count);
    }

    if(mi_buffer)
    {
        StatMI();
        mi_buffer->WriteNode(rn_list.GetData(),node_count,mi_set);
    }
}

void MaterialRenderList::RenderItem::Set(Renderable *ri)
{
    pipeline    =ri->GetPipeline();
    mi          =ri->GetMaterialInstance();
    vid         =ri->GetVertexInputData();
}

void MaterialRenderList::StatMI()
{
    mi_set.Clear();

    for(RenderNode &rn:rn_list)
        mi_set.Add(rn.ri->GetMaterialInstance());

    if(mi_set.GetCount()>material->GetMIMaxCount())
    {
        //超出最大数量了怎么办？？？
    }
}

void MaterialRenderList::Stat()
{
    const uint count=rn_list.GetCount();
    RenderNode *rn=rn_list.GetData();

    ri_array.Clear();
    ri_array.Alloc(count);

    RenderItem *ri=ri_array.GetData();

    ri_count=1;

    ri->first=0;
    ri->count=1;
    ri->Set(rn->ri);

    last_pipeline   =ri->pipeline;
    last_vid        =ri->vid;

    ++rn;

    for(uint i=1;i<count;i++)
    {
        if(last_pipeline==rn->ri->GetPipeline())
            if(last_vid->Comp(rn->ri->GetVertexInputData()))
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
        last_vid        =ri->vid;

        ++rn;
    }
}

bool MaterialRenderList::Bind(const VertexInputData *vid,const uint ri_index)
{
    //binding号都是在VertexInput::CreateVIL时连续紧密排列生成的，所以bind时first_binding写0就行了。

    //const VIL *vil=last_vil;

    //if(vil->GetCount(VertexInputGroup::Basic)!=vid->binding_count)
    //    return(false);                                                  //这里基本不太可能，因为CreateRenderable时就会检查值是否一样

    vbo_list->Restart();

    //Basic组，它所有的VAB信息均来自于Primitive，由vid参数传递进来
    {
        vbo_list->Add(vid->buffer_list,vid->buffer_offset,vid->binding_count);
    }

    if(l2w_buffer)//LocalToWorld组，由RenderList合成
    {
        for(uint i=0;i<4;i++)
            l2w_buffer_size[i]=ri_index*16;                        //mat4每列都是rgba32f，自然是16字节

        vbo_list->Add(l2w_buffer->GetVBO(),l2w_buffer_size,4);
    }

    if(mi_buffer) //材质实例组
        vbo_list->Add(mi_buffer->GetVBO(),MI_VAB_STRIDE_BYTES*ri_index);

    //if(!vbo_list.IsFull()) //Joint组，暂未支持
    //{
    //    const uint joint_id_binding_count=vil->GetCount(VertexInputGroup::JointID);

    //    if(joint_id_binding_count>0)                                        //有矩阵信息
    //    {
    //        count+=joint_id_binding_count;

    //        if(count<binding_count) //JointWeight组
    //        {
    //            const uint joing_weight_binding_count=vil->GetCount(VertexInputGroup::JointWeight);

    //            if(joing_weight_binding_count!=1)
    //            {
    //                ++count;
    //            }
    //            else //JointWieght不为1是个bug，除非未来支持8权重
    //            {
    //                return(false);
    //            }
    //        }
    //        else //有JointID没有JointWeight? 这是个BUG
    //        {
    //            return(false);
    //        }
    //    }
    //}

    //if(count!=binding_count)
    //{
    //    //还有没支持的绑定组？？？？

    //    return(false);
    //}

    cmd_buf->BindVBO(vbo_list);

    return(true);
}

void MaterialRenderList::Render(RenderItem *ri)
{
    if(last_pipeline!=ri->pipeline)
    {
        cmd_buf->BindPipeline(ri->pipeline);
        last_pipeline=ri->pipeline;

        last_vid=nullptr;

        //这里未来尝试换pipeline同时不换mi/primitive是否需要重新绑定mi/primitive
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

    RenderItem *ri=ri_array.GetData();

    last_pipeline   =nullptr;
    last_vid        =nullptr;

    if(mi_buffer)
        mi_buffer->Bind(material);

    cmd_buf->BindDescriptorSets(material);

    for(uint i=0;i<ri_count;i++)
    {
        Render(ri);
        ++ri;
    }
}
VK_NAMESPACE_END
