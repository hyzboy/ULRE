#include<hgl/graph/PipelineMaterialBatch.h>
#include<hgl/graph/mesh/Mesh.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/util/sort/Sort.h>
#include"InstanceAssignmentBuffer.h"
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
        assign_buffer=new InstanceAssignmentBuffer(device,pm_index.material);
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

    Clear();
}

void PipelineMaterialBatch::Add(DrawNode *node)
{
    if(!node) return;

    node->index=draw_nodes.GetCount();

    NodeTransform *tf = node->GetOwner();
    if (camera_info && tf)
    {
        node->world_position     =tf->GetWorldPosition();
        node->to_camera_distance =length(camera_info->pos,node->world_position);
    }
    else
    {
        node->world_position     =Vector3f(0,0,0);
        node->to_camera_distance =0;
    }

    node->transform_version=tf?tf->GetTransformVersion():0;
    node->transform_index=0;

    draw_nodes.Add(node);
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
    uint32 transform_version=0;
    DrawNode **node=draw_nodes.GetData();

    for(int i=0;i<node_count;i++)
    {
        auto *tf=(*node)->GetTransform();
        transform_version=tf?tf->GetTransformVersion():0;

        if((*node)->transform_version!=transform_version)       //版本不对，需要更新
        {
            if(first==-1)
            {
                first=(*node)->transform_index;
            }
            
            last=(*node)->transform_index;

            (*node)->transform_version=transform_version;

            transform_dirty_nodes.Add(*node);

            ++update_count;
        }

        ++node;
    }

    if(update_count>0)
    {
        assign_buffer->UpdateTransformData(transform_dirty_nodes,first,last);
        transform_dirty_nodes.Clear();
    }
}

void PipelineMaterialBatch::UpdateMaterialInstanceData(MeshComponent *mesh_component)
{
    if(!mesh_component)return;

    if(!assign_buffer)
        return;
    
    const int node_count=draw_nodes.GetCount();

    if(node_count<=0)return;
    DrawNode **node=draw_nodes.GetData();

    for(int i=0;i<node_count;i++)
    {
        auto *mc=dynamic_cast<MeshComponentDrawNode *>(*node);
        if(mc && mc->GetOwner()==mesh_component)
        {
            assign_buffer->UpdateMaterialInstanceData(*node);
            return;
        }

        ++node;
    }
}

void PipelineMaterialBatch::DrawBatch::Set(Mesh *mesh)
{
    mesh_data_buffer=mesh->GetDataBuffer();
    mesh_render_data=mesh->GetRenderData();
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

void PipelineMaterialBatch::WriteICB(VkDrawIndirectCommand *dicp,DrawBatch *batch)
{
    dicp->vertexCount   =batch->mesh_render_data->vertex_count;
    dicp->instanceCount =batch->instance_count;
    dicp->firstVertex   =batch->mesh_render_data->vertex_offset;
    dicp->firstInstance =batch->first_instance;
}

void PipelineMaterialBatch::WriteICB(VkDrawIndexedIndirectCommand *diicp,DrawBatch *batch)
{
    diicp->indexCount   =batch->mesh_render_data->index_count;
    diicp->instanceCount=batch->instance_count;
    diicp->firstIndex   =batch->mesh_render_data->first_index;
    diicp->vertexOffset =batch->mesh_render_data->vertex_offset;
    diicp->firstInstance=batch->first_instance;
}

void PipelineMaterialBatch::BuildBatches()
{
    const uint count=draw_nodes.GetCount();
    DrawNode **node=draw_nodes.GetData();

    ReallocICB();

    VkDrawIndirectCommand *         dicp    =icb_draw->MapCmd();
    VkDrawIndexedIndirectCommand *  diicp   =icb_draw_indexed->MapCmd();

    draw_batches.Clear();
    draw_batches.Reserve(count);

    DrawBatch * batch   =draw_batches.GetData();
    Mesh *      mesh    =(*node)->GetMesh();

    draw_batches_count=1;

    batch->first_instance=0;
    batch->instance_count=1;
    batch->Set(mesh);

    last_data_buffer=batch->mesh_data_buffer;
    last_vdm        =batch->mesh_data_buffer->vdm;
    last_render_data=batch->mesh_render_data;

    ++node;

    for(uint i=1;i<count;i++)
    {
        mesh=(*node)->GetMesh();

        if(*last_data_buffer==*mesh->GetDataBuffer())
            if(*last_render_data==*mesh->GetRenderData())
            {
                ++batch->instance_count;
                ++node;
                continue;
            }

        if(batch->mesh_data_buffer->vdm)
        {
            if(batch->mesh_data_buffer->ibo)
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

        last_data_buffer=batch->mesh_data_buffer;
        last_vdm        =batch->mesh_data_buffer->vdm;
        last_render_data=batch->mesh_render_data;

        ++node;
    }

    if(batch->mesh_data_buffer->vdm)
    {
        if(batch->mesh_data_buffer->ibo)
            WriteICB(diicp,batch);
        else
            WriteICB(dicp,batch);
    }

    icb_draw->Unmap();
    icb_draw_indexed->Unmap();
}

bool PipelineMaterialBatch::BindVAB(const DrawBatch *batch)
{
    vab_list->Restart();

    if(!vab_list->Add(batch->mesh_data_buffer))
        return(false);

    if (assign_buffer) //L2W/MI分发组
    {
        if(!vab_list->Add(assign_buffer->GetVAB(),0))
            return(false);
    }

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
    if(!last_data_buffer||*(batch->mesh_data_buffer)!=*last_data_buffer)        //换buf了
    {
        if(indirect_draw_count)                 //如果有间接绘制的数据，赶紧给画了
            ProcIndirectRender();

        last_data_buffer=batch->mesh_data_buffer;
        last_render_data=nullptr;

        if(!BindVAB(batch))
        {
            return(false);
        }

        if(batch->mesh_data_buffer->ibo)
            cmd_buf->BindIBO(batch->mesh_data_buffer->ibo);
    }

    if(batch->mesh_data_buffer->vdm)
    {
        if(indirect_draw_count==0)
            first_indirect_draw_index=batch->first_instance;

        ++indirect_draw_count;
    }
    else
    {
        cmd_buf->Draw(batch->mesh_data_buffer,batch->mesh_render_data,batch->instance_count,batch->first_instance);
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
