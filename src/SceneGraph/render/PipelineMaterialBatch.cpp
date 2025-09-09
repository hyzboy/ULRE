#include<hgl/graph/PipelineMaterialBatch.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/util/sort/Sort.h>
#include"RenderAssignBuffer.h"
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/component/MeshComponent.h>

VK_NAMESPACE_BEGIN
PipelineMaterialBatch::PipelineMaterialBatch(VulkanDevice *d,bool l2w,const PipelineMaterialIndex &rpi)
{
    device=d;
    cmd_buf=nullptr;
    pm_index=rpi;

    camera_info=nullptr;

    if(rpi.material->hasLocalToWorld()
     ||rpi.material->hasMI())
    {
        assign_buffer=new RenderAssignBuffer(device,pm_index.material);
    }
    else
    {
        assign_buffer=nullptr;
    }

    icb_draw=nullptr;
    icb_draw_indexed=nullptr;

    draw_batches_count=0;

    vab_list=new VABList(pm_index.material->GetVertexInput()->GetCount());

    last_data_buffer=nullptr;
    last_vdm=nullptr;
    last_render_data=nullptr;

    first_indirect_draw_index=-1;
    indirect_draw_count=0;
}

PipelineMaterialBatch::~PipelineMaterialBatch()
{
    SAFE_CLEAR(icb_draw_indexed)
    SAFE_CLEAR(icb_draw)

    SAFE_CLEAR(vab_list);
    SAFE_CLEAR(assign_buffer);
}

void PipelineMaterialBatch::Add(MeshComponent *smc)
{
    if(!smc)
        return;

    DrawNode rn;

    rn.index            =draw_nodes.GetCount();

    rn.sm_component     =smc;

    rn.l2w_version      =smc->GetTransformVersion();
    rn.l2w_index        =0;

    rn.world_position   =smc->GetWorldPosition();

    if(camera_info)
        rn.to_camera_distance=length(camera_info->pos,rn.world_position);
    else
        rn.to_camera_distance=0;

    draw_nodes.Add(rn);
}

void PipelineMaterialBatch::Finalize()
{
    //排序
    Sort(draw_nodes.GetArray());

    const uint node_count=draw_nodes.GetCount();

    if(node_count<=0)return;

    BuildBatches();

    if(assign_buffer)
        assign_buffer->WriteNode(draw_nodes);
}

void PipelineMaterialBatch::UpdateTransformData()
{
    if(!assign_buffer)
        return;

    transform_dirty_nodes.Clear();

    const int node_count=draw_nodes.GetCount();

    if(node_count<=0)return;

    int first=-1,last=-1;
    int update_count=0;
    uint32 l2w_version=0;
    DrawNode *node=draw_nodes.GetData();

    for(int i=0;i<node_count;i++)
    {
        l2w_version=node->sm_component->GetTransformVersion();

        if(node->l2w_version!=l2w_version)       //版本不对，需要更新
        {
            if(first==-1)
            {
                first=node->l2w_index;
            }
            
            last=node->l2w_index;

            node->l2w_version=l2w_version;

            transform_dirty_nodes.Add(node);

            ++update_count;
        }

        ++node;
    }

    if(update_count>0)
    {
        assign_buffer->UpdateLocalToWorld(transform_dirty_nodes,first,last);
        transform_dirty_nodes.Clear();
    }
}

void PipelineMaterialBatch::UpdateMaterialInstanceData(MeshComponent *smc)
{
    if(!smc)return;

    if(!assign_buffer)
        return;
    
    const int node_count=draw_nodes.GetCount();

    if(node_count<=0)return;
    DrawNode *node=draw_nodes.GetData();

    for(int i=0;i<node_count;i++)
    {
        if(node->sm_component==smc)
        {
            assign_buffer->UpdateMaterialInstanceData(node);
            return;
        }

        ++node;
    }
}

void PipelineMaterialBatch::DrawBatch::Set(Mesh *ri)
{
    mi      =ri->GetMaterialInstance();
    pdb     =ri->GetDataBuffer();
    prd     =ri->GetRenderData();
}

void PipelineMaterialBatch::ReallocICB()
{
    const uint32_t icb_new_count=power_to_2(draw_nodes.GetCount());

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

void PipelineMaterialBatch::WriteICB(VkDrawIndirectCommand *dicp,DrawBatch *ri)
{
    dicp->vertexCount   =ri->prd->vertex_count;
    dicp->instanceCount =ri->instance_count;
    dicp->firstVertex   =ri->prd->vertex_offset;
    dicp->firstInstance =ri->first_instance;
}

void PipelineMaterialBatch::WriteICB(VkDrawIndexedIndirectCommand *diicp,DrawBatch *ri)
{
    diicp->indexCount   =ri->prd->index_count;
    diicp->instanceCount=ri->instance_count;
    diicp->firstIndex   =ri->prd->first_index;
    diicp->vertexOffset =ri->prd->vertex_offset;
    diicp->firstInstance=ri->first_instance;
}

void PipelineMaterialBatch::BuildBatches()
{
    const uint count=draw_nodes.GetCount();
    DrawNode *node=draw_nodes.GetData();

    ReallocICB();

    VkDrawIndirectCommand *dicp=icb_draw->MapCmd();
    VkDrawIndexedIndirectCommand *diicp=icb_draw_indexed->MapCmd();

    draw_batches.Clear();
    draw_batches.Reserve(count);

    DrawBatch *batch=draw_batches.GetData();
    Mesh *mesh=node->sm_component->GetMesh();

    draw_batches_count=1;

    batch->first_instance=0;
    batch->instance_count=1;
    batch->Set(mesh);

    last_data_buffer=batch->pdb;
    last_vdm        =batch->pdb->vdm;
    last_render_data=batch->prd;

    ++node;

    for(uint i=1;i<count;i++)
    {
        mesh=node->sm_component->GetMesh();

        if(*last_data_buffer==*mesh->GetDataBuffer())
            if(*last_render_data==*mesh->GetRenderData())
            {
                ++batch->instance_count;
                ++node;
                continue;
            }

        if(batch->pdb->vdm)
        {
            if(batch->pdb->ibo)
                WriteICB(diicp,batch);
            else
                WriteICB(dicp,batch);

            ++dicp;
            ++diicp;
        }

        ++draw_batches_count;
        ++batch;

        batch->first_instance=i;
        batch->instance_count=1;
        batch->Set(mesh);

        last_data_buffer=batch->pdb;
        last_vdm        =batch->pdb->vdm;
        last_render_data=batch->prd;

        ++node;
    }

    if(batch->pdb->vdm)
    {
        if(batch->pdb->ibo)
            WriteICB(diicp,batch);
        else
            WriteICB(dicp,batch);
    }

    icb_draw->Unmap();
    icb_draw_indexed->Unmap();
}

bool PipelineMaterialBatch::BindVAB(const DrawBatch *batch)
{
    const MeshDataBuffer *  pdb     =batch->pdb;
    const uint              ri_index=batch->first_instance;

    //binding号都是在VertexInput::CreateVIL时连续紧密排列生成的，所以bind时first_binding写0就行了。

    //const VIL *vil=last_vil;

    //if(vil->GetCount(VertexInputGroup::Basic)!=prb->vab_count)
    //    return(false);                                                  //这里基本不太可能，因为CreateMesh时就会检查值是否一样

    vab_list->Restart();

    //Basic组，它所有的VAB信息均来自于Primitive，由vid参数传递进来
    {
        if(!vab_list->Add(pdb->vab_list,pdb->vab_offset,pdb->vab_count))
        {
            //这个情况很严重哦！
            return(false);
        }
    }

    if (assign_buffer) //L2W/MI分发组
    {
        if(!vab_list->Add(assign_buffer->GetVAB(),0))//ASSIGN_VAB_STRIDE_BYTES*ri_index);
        {
            //一般出现这个情况是因为材质中没有配置需要L2W/或是MIData
            return(false);
        }
    }

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

void PipelineMaterialBatch::ProcIndirectRender()
{    
    if(last_data_buffer->ibo)
        icb_draw_indexed->DrawIndexed(*cmd_buf,first_indirect_draw_index,indirect_draw_count);
    else
        icb_draw->Draw(*cmd_buf,first_indirect_draw_index,indirect_draw_count);

    first_indirect_draw_index=-1;
    indirect_draw_count=0;
}

bool PipelineMaterialBatch::Draw(DrawBatch *batch)
{
    if(!last_data_buffer||*(batch->pdb)!=*last_data_buffer)        //换buf了
    {
        if(indirect_draw_count)                 //如果有间接绘制的数据，赶紧给画了
            ProcIndirectRender();

        last_data_buffer=batch->pdb;
        last_render_data=nullptr;

        if(!BindVAB(batch))
        {
            //这个问题很严重哦
            return(false);
        }

        if(batch->pdb->ibo)
            cmd_buf->BindIBO(batch->pdb->ibo);
    }

    if(batch->pdb->vdm)
    {
        if(indirect_draw_count==0)
            first_indirect_draw_index=batch->first_instance;

        ++indirect_draw_count;
    }
    else
    {
        cmd_buf->Draw(batch->pdb,batch->prd,batch->instance_count,batch->first_instance);
    }

    return(true);
}

void PipelineMaterialBatch::Render(RenderCmdBuffer *rcb)
{
    if(!rcb)return;
    const uint count=draw_nodes.GetCount();

    if(count<=0)return;

    if(draw_batches_count<=0)return;

    cmd_buf=rcb;

    cmd_buf->BindPipeline(pm_index.pipeline);

    last_data_buffer=nullptr;
    last_vdm        =nullptr;
    last_render_data=nullptr;

    if(assign_buffer)
        assign_buffer->Bind(pm_index.material);

    cmd_buf->BindDescriptorSets(pm_index.material);

    DrawBatch *batch=draw_batches.GetData();
    for(uint i=0;i<draw_batches_count;i++)
    {
        Draw(batch);
        ++batch;
    }
    
    if(indirect_draw_count)                 //如果有间接绘制的数据，赶紧给画了
        ProcIndirectRender();
}
VK_NAMESPACE_END
