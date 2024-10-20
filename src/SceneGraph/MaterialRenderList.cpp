#include<hgl/graph/MaterialRenderList.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/util/sort/Sort.h>
#include"RenderAssignBuffer.h"
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/CameraInfo.h>

/**
* 
* 理论上讲，我们需要按以下顺序排序
*
*   for(material)
*       for(pipeline)
*           for(material_instance)
*               for(vab)
* 
* 
*  关于Indirect Command Buffer

    建立一个大的IndirectCommandBuffer，用于存放所有的渲染指令，包括那些不能使用Indirect渲染的。
    这样就可以保证所有的渲染操作就算要切VBO，也不需要切换INDIRECT缓冲区，定位指令也很方便。
*/

template<> 
int Comparator<hgl::graph::RenderNode>::compare(const hgl::graph::RenderNode &obj_one,const hgl::graph::RenderNode &obj_two) const
{
    hgl::int64 off;

    hgl::graph::Renderable *ri_one=obj_one.scene_node->GetRenderable();
    hgl::graph::Renderable *ri_two=obj_two.scene_node->GetRenderable();

    auto *prim_one=ri_one->GetPrimitive();
    auto *prim_two=ri_two->GetPrimitive();

    //比较VDM

    if(prim_one->GetVDM())      //有VDM
    {
        off=prim_one->GetVDM()
           -prim_two->GetVDM();

        if(off)
            return off;

        //比较模型
        {
            off=prim_one
               -prim_two;

            if(off)
            {
                off=prim_one->GetVertexOffset()-prim_two->GetVertexOffset();        //保证vertex offset小的在前面

                return off;
            }
        }
    }

    //比较距离。。。。。。。。。。。。。。。。。。。。。还不知道这个是正了还是反了，等测出来确认后修改下面的返回值和这里的注释

    float foff=obj_one.to_camera_distance
              -obj_two.to_camera_distance;

    if(foff>0)
        return 1;
    else
        return -1;
}

VK_NAMESPACE_BEGIN
MaterialRenderList::MaterialRenderList(GPUDevice *d,bool l2w,const RenderPipelineIndex &rpi)
{
    device=d;
    cmd_buf=nullptr;
    rp_index=rpi;

    camera_info=nullptr;

    assign_buffer=new RenderAssignBuffer(device,rp_index.material);

    vab_list=new VABList(rp_index.material->GetVertexInput()->GetCount());

    icb_draw=nullptr;
    icb_draw_indexed=nullptr;
}

MaterialRenderList::~MaterialRenderList()
{
    SAFE_CLEAR(icb_draw_indexed)
    SAFE_CLEAR(icb_draw)

    SAFE_CLEAR(vab_list);
    SAFE_CLEAR(assign_buffer);
}

void MaterialRenderList::Add(SceneNode *sn)
{
    RenderNode rn;

    rn.index            =rn_list.GetCount();

    rn.scene_node       =sn;

    rn.l2w_version      =sn->GetLocalToWorldMatrixVersion();
    rn.l2w_index        =0;

    rn.world_position   =sn->GetWorldPosition();

    if(camera_info)
        rn.to_camera_distance=length(camera_info->pos,rn.world_position);
    else
        rn.to_camera_distance=0;

    rn_list.Add(rn);
}

void MaterialRenderList::End()
{
    //排序
    Sort(rn_list.GetArray());

    const uint node_count=rn_list.GetCount();

    if(node_count<=0)return;

    Stat();

    if(assign_buffer)
        assign_buffer->WriteNode(rn_list);
}

void MaterialRenderList::UpdateLocalToWorld()
{
    if(!assign_buffer)
        return;

    rn_update_l2w_list.Clear();

    const int node_count=rn_list.GetCount();

    if(node_count<=0)return;

    int first=-1,last=-1;
    int update_count=0;
    uint32 l2w_version=0;
    RenderNode *rn=rn_list.GetData();

    for(int i=0;i<node_count;i++)
    {
        l2w_version=rn->scene_node->GetLocalToWorldMatrixVersion();

        if(rn->l2w_version!=l2w_version)       //版本不对，需要更新
        {
            if(first==-1)
            {
                first=rn->l2w_index;
            }
            
            last=rn->l2w_index;

            rn->l2w_version=l2w_version;

            rn_update_l2w_list.Add(rn);

            ++update_count;
        }

        ++rn;
    }

    if(update_count>0)
    {
        assign_buffer->UpdateLocalToWorld(rn_update_l2w_list,first,last);
        rn_update_l2w_list.Clear();
    }
}

void MaterialRenderList::UpdateMaterialInstance(SceneNode *sn)
{
    if(!sn)return;

    if(!assign_buffer)
        return;
    
    const int node_count=rn_list.GetCount();

    if(node_count<=0)return;
    RenderNode *rn=rn_list.GetData();

    for(int i=0;i<node_count;i++)
    {
        if(rn->scene_node==sn)
        {
            assign_buffer->UpdateMaterialInstance(rn);
            return;
        }

        ++rn;
    }
}

void MaterialRenderList::RenderItem::Set(Renderable *ri)
{
    mi      =ri->GetMaterialInstance();
    pdb     =ri->GetDataBuffer();
    prd     =ri->GetRenderData();
}

void MaterialRenderList::ReallocICB()
{
    const uint32_t icb_new_count=power_to_2(rn_list.GetCount());

    if(icb_draw)
    {
        if(icb_new_count<=icb_draw->GetMaxCount())
            return;

        delete icb_draw;
        icb_draw=nullptr;

        delete icb_draw_indexed;
        icb_draw_indexed=nullptr;
    }

    icb_draw=device->CreateIndirectDrawBuffer(icb_new_count);
    icb_draw_indexed=device->CreateIndirectDrawIndexedBuffer(icb_new_count);
}

void MaterialRenderList::WriteICB(VkDrawIndirectCommand *dicp,RenderItem *ri)
{    
    dicp->vertexCount   =ri->prd->vertex_count;
    dicp->instanceCount =ri->instance_count;
    dicp->firstVertex   =ri->prd->vertex_offset;
    dicp->firstInstance =ri->first_instance;
}

void MaterialRenderList::WriteICB(VkDrawIndexedIndirectCommand *diicp,RenderItem *ri)
{
    diicp->indexCount   =ri->prd->index_count;
    diicp->instanceCount=ri->instance_count;
    diicp->firstIndex   =ri->prd->first_index;
    diicp->vertexOffset =ri->prd->vertex_offset;
    diicp->firstInstance=ri->first_instance;
}

void MaterialRenderList::Stat()
{
    const uint count=rn_list.GetCount();
    RenderNode *rn=rn_list.GetData();

    ReallocICB();

    VkDrawIndirectCommand *dicp=icb_draw->MapCmd();
    VkDrawIndexedIndirectCommand *diicp=icb_draw_indexed->MapCmd();

    ri_array.Clear();
    ri_array.Alloc(count);

    RenderItem *ri=ri_array.GetData();
    Renderable *ro=rn->scene_node->GetRenderable();

    ri_count=1;

    ri->first_instance=0;
    ri->instance_count=1;
    ri->Set(ro);

    last_data_buffer=ri->pdb;
    last_vdm        =ri->pdb->vdm;
    last_render_data=ri->prd;

    ++rn;

    for(uint i=1;i<count;i++)
    {
        ro=rn->scene_node->GetRenderable();

        if(last_data_buffer->Comp(ro->GetDataBuffer()))
            if(last_render_data->_Comp(ro->GetRenderData())==0)
            {
                ++ri->instance_count;
                ++rn;
                continue;
            }

        if(ri->pdb->vdm)
        {
            if(ri->pdb->ibo)
                WriteICB(diicp,ri);
            else
                WriteICB(dicp,ri);

            ++dicp;
            ++diicp;
        }

        ++ri_count;
        ++ri;

        ri->first_instance=i;
        ri->instance_count=1;
        ri->Set(ro);

        last_data_buffer=ri->pdb;
        last_vdm        =ri->pdb->vdm;
        last_render_data=ri->prd;

        ++rn;
    }

    if(ri->pdb->vdm)
    {
        if(ri->pdb->ibo)
            WriteICB(diicp,ri);
        else
            WriteICB(dicp,ri);
    }

    icb_draw->Unmap();
    icb_draw_indexed->Unmap();
}

bool MaterialRenderList::BindVAB(const PrimitiveDataBuffer *pdb,const uint ri_index)
{
    //binding号都是在VertexInput::CreateVIL时连续紧密排列生成的，所以bind时first_binding写0就行了。

    //const VIL *vil=last_vil;

    //if(vil->GetCount(VertexInputGroup::Basic)!=prb->vab_count)
    //    return(false);                                                  //这里基本不太可能，因为CreateRenderable时就会检查值是否一样

    vab_list->Restart();

    //Basic组，它所有的VAB信息均来自于Primitive，由vid参数传递进来
    {
        vab_list->Add(pdb->vab_list,
                      pdb->vab_offset,
                      pdb->vab_count);
    }

    if(assign_buffer) //L2W/MI分发组
        vab_list->Add(assign_buffer->GetVAB(),0);//ASSIGN_VAB_STRIDE_BYTES*ri_index);

    //if(!vab_list.IsFull()) //Joint组，暂未支持
    //{
    //    const uint joint_id_binding_count=vil->GetCount(VertexInputGroup::JointID);

    //    if(joint_id_binding_count>0)                                        //有矩阵信息
    //    {
    //        count+=joint_id_binding_count;

    //        if(count<vab_count) //JointWeight组
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

    //if(count!=vab_count)
    //{
    //    //还有没支持的绑定组？？？？

    //    return(false);
    //}

    cmd_buf->BindVAB(vab_list);

    return(true);
}

void MaterialRenderList::ProcIndirectRender()
{    
    if(last_data_buffer->ibo)
        icb_draw_indexed->DrawIndexed(*cmd_buf,first_indirect_draw_index,indirect_draw_count);
    else
        icb_draw->Draw(*cmd_buf,first_indirect_draw_index,indirect_draw_count);

    first_indirect_draw_index=-1;
    indirect_draw_count=0;
}

void MaterialRenderList::Render(RenderItem *ri)
{
    if(!ri->pdb->Comp(last_data_buffer))        //换buf了
    {
        if(indirect_draw_count)                 //如果有间接绘制的数据，赶紧给画了
            ProcIndirectRender();

        last_data_buffer=ri->pdb;
        last_render_data=nullptr;

        BindVAB(ri->pdb,ri->first_instance);

        if(ri->pdb->ibo)
            cmd_buf->BindIBO(ri->pdb->ibo);
    }

    if(ri->pdb->vdm)
    {
        if(indirect_draw_count==0)
            first_indirect_draw_index=ri->first_instance;

        ++indirect_draw_count;
    }
    else
    {
        cmd_buf->Draw(ri->pdb,ri->prd,ri->instance_count,ri->first_instance);
    }
}

void MaterialRenderList::Render(RenderCmdBuffer *rcb)
{
    if(!rcb)return;
    const uint count=rn_list.GetCount();

    if(count<=0)return;

    if(ri_count<=0)return;

    cmd_buf=rcb;

    cmd_buf->BindPipeline(rp_index.pipeline);

    last_data_buffer=nullptr;
    last_vdm        =nullptr;
    last_render_data=nullptr;

    if(assign_buffer)
        assign_buffer->Bind(rp_index.material);

    cmd_buf->BindDescriptorSets(rp_index.material);

    RenderItem *ri=ri_array.GetData();
    for(uint i=0;i<ri_count;i++)
    {
        Render(ri);
        ++ri;
    }
    
    if(indirect_draw_count)                 //如果有间接绘制的数据，赶紧给画了
        ProcIndirectRender();
}
VK_NAMESPACE_END
