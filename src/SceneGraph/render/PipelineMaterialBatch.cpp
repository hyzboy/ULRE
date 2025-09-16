#include<hgl/graph/PipelineMaterialBatch.h>
#include<hgl/graph/Mesh.h>
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
}

void PipelineMaterialBatch::Add(MeshComponent *mesh_component)
{
    if(!mesh_component)
        return;

    DrawNode node;

    node.index            =draw_nodes.GetCount();

    node.mesh_component   =mesh_component;

    node.transform_version=mesh_component->GetTransformVersion();
    node.transform_index  =0;

    node.world_position   =mesh_component->GetWorldPosition();

    if(camera_info)
        node.to_camera_distance=length(camera_info->pos,node.world_position);
    else
        node.to_camera_distance=0;

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
    DrawNode *node=draw_nodes.GetData();

    for(int i=0;i<node_count;i++)
    {
        transform_version=node->mesh_component->GetTransformVersion();

        if(node->transform_version!=transform_version)       //版本不对，需要更新
        {
            if(first==-1)
            {
                first=node->transform_index;
            }
            
            last=node->transform_index;

            node->transform_version=transform_version;

            transform_dirty_nodes.Add(node);

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
    DrawNode *node=draw_nodes.GetData();

    for(int i=0;i<node_count;i++)
    {
        if(node->mesh_component==mesh_component)
        {
            assign_buffer->UpdateMaterialInstanceData(node);
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
    DrawNode *node=draw_nodes.GetData();

    ReallocICB();

    VkDrawIndirectCommand *         dicp    =icb_draw->MapCmd();
    VkDrawIndexedIndirectCommand *  diicp   =icb_draw_indexed->MapCmd();

    draw_batches.Clear();
    draw_batches.Reserve(count);

    DrawBatch * batch   =draw_batches.GetData();
    Mesh *      mesh    =node->mesh_component->GetMesh();

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
        mesh=node->mesh_component->GetMesh();

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
//    const uint              ri_index        =batch->first_instance;

    //binding号都是在VertexInput::CreateVIL时连续紧密排列生成的，所以bind时first_binding写0就行了。

    //const VIL *vil=last_vil;

    //if(vil->GetCount(VertexInputGroup::Basic)!=prb->vab_count)
    //    return(false);                                                  //这里基本不太可能，因为CreateMesh时就会检查值是否一样

    vab_list->Restart();

    //Basic组，它所有的VAB信息均来自于Primitive，由vid参数传递进来
    {
        if(!vab_list->Add(batch->mesh_data_buffer))
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
    if(!last_data_buffer||*(batch->mesh_data_buffer)!=*last_data_buffer)        //换buf了
    {
        if(indirect_draw_count)                 //如果有间接绘制的数据，赶紧给画了
            ProcIndirectRender();

        last_data_buffer=batch->mesh_data_buffer;
        last_render_data=nullptr;

        if(!BindVAB(batch))
        {
            //这个问题很严重哦
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
