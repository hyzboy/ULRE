#include<hgl/graph/PipelineMaterialBatch.h>
#include<hgl/graph/PipelineMaterialRenderer.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/util/sort/Sort.h>
#include"InstanceAssignmentBuffer.h"
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/component/PrimitiveComponent.h>

VK_NAMESPACE_BEGIN

PipelineMaterialBatch::PipelineMaterialBatch(VulkanDevice *d, bool l2w, const PipelineMaterialIndex &rpi)
    : device(d)
    , pm_index(rpi)
    , camera_info(nullptr)
    , assign_buffer(nullptr)
    , draw_batches_count(0)
    , icb_draw(nullptr)
    , icb_draw_indexed(nullptr)
    , renderer(nullptr)
{
    // 如果材质需要 LocalToWorld 或材质实例数据，创建分配缓冲
    if (rpi.material->hasLocalToWorld() || rpi.material->hasMI())
    {
        assign_buffer = new InstanceAssignmentBuffer(device, pm_index.material);
    }

    // 创建渲染器
    renderer = new PipelineMaterialRenderer(pm_index.material, pm_index.pipeline);
}

PipelineMaterialBatch::~PipelineMaterialBatch()
{
    SAFE_CLEAR(icb_draw_indexed)
    SAFE_CLEAR(icb_draw)
    SAFE_CLEAR(assign_buffer);
    SAFE_CLEAR(renderer);

    Clear();
}

void PipelineMaterialBatch::Add(DrawNode *node)
{
    if (!node) return;

    node->index = draw_nodes.GetCount();

    // 初始化节点的空间数据
    NodeTransform *tf = node->GetTransform();
    if (camera_info && tf)
    {
        node->world_position = tf->GetWorldPosition();
        node->to_camera_distance = math::length(camera_info->pos, node->world_position);
    }
    else
    {
        node->world_position = math::Vector3f(0, 0, 0);
        node->to_camera_distance = 0;
    }

    node->transform_version = tf ? tf->GetTransformVersion() : 0;
    node->transform_index = 0;

    draw_nodes.Add(node);
}

void PipelineMaterialBatch::Finalize()
{
    // 排序节点以优化渲染顺序
    Sort(draw_nodes.GetArray());

    const uint node_count = draw_nodes.GetCount();
    if (node_count <= 0) return;

    // 构建绘制批次
    BuildBatches();

    // 写入实例数据到缓冲
    if (assign_buffer)
        assign_buffer->WriteNode(draw_nodes);
}

void PipelineMaterialBatch::UpdateTransformData()
{
    if (!assign_buffer) return;

    transform_dirty_nodes.Clear();

    const int node_count = draw_nodes.GetCount();
    if (node_count <= 0) return;

    // 收集需要更新的节点
    int first_index = -1;
    int last_index = -1;

    DrawNode **node_ptr = draw_nodes.GetData();
    for (int i = 0; i < node_count; i++, node_ptr++)
    {
        DrawNode *node = *node_ptr;
        NodeTransform *tf = node->GetTransform();
        
        // 获取当前变换版本号
        const uint32 current_version = tf ? tf->GetTransformVersion() : 0;

        // 检查版本号是否变化
        if (node->transform_version != current_version)
        {
            // 更新版本号
            node->transform_version = current_version;

            // 更新范围索引
            const int transform_idx = node->transform_index;
            if (first_index == -1)
            {
                first_index = transform_idx;
            }
            last_index = transform_idx;

            // 添加到脏列表
            transform_dirty_nodes.Add(node);
        }
    }

    // 批量更新变换数据
    if (!transform_dirty_nodes.IsEmpty())
    {
        assign_buffer->UpdateTransformData(transform_dirty_nodes, first_index, last_index);
        transform_dirty_nodes.Clear();
    }
}

void PipelineMaterialBatch::UpdateMaterialInstanceData(PrimitiveComponent *prim_component)
{
    // 提前返回，减少嵌套
    if (!prim_component) return;
    if (!assign_buffer) return;
    
    const int node_count = draw_nodes.GetCount();
    if (node_count <= 0) return;

    DrawNode **node_ptr = draw_nodes.GetData();
    for (int i = 0; i < node_count; i++, node_ptr++)
    {
        auto *mc = dynamic_cast<DrawNodePrimitive *>(*node_ptr);

        if (mc && mc->GetPrimitiveComponent() == prim_component)
        {
            assign_buffer->UpdateMaterialInstanceData(*node_ptr);
            return;
        }
    }
}

void PipelineMaterialBatch::ReallocICB()
{
    const uint32_t icb_new_count = power_to_2(draw_nodes.GetCount());

    // 如果现有缓冲足够大，直接返回
    if (icb_draw && icb_new_count <= icb_draw->GetMaxCount())
    {
        return;
    }

    // 删除旧缓冲
    SAFE_CLEAR(icb_draw);
    SAFE_CLEAR(icb_draw_indexed);

    // 创建新缓冲
    icb_draw = device->CreateIndirectDrawBuffer(icb_new_count);
    icb_draw_indexed = device->CreateIndirectDrawIndexedBuffer(icb_new_count);
}

void PipelineMaterialBatch::WriteICB(VkDrawIndirectCommand *draw_cmd, DrawBatch *batch)
{
    draw_cmd->vertexCount = batch->geom_draw_range->vertex_count;
    draw_cmd->instanceCount = batch->instance_count;
    draw_cmd->firstVertex = batch->geom_draw_range->vertex_offset;
    draw_cmd->firstInstance = batch->first_instance;
}

void PipelineMaterialBatch::WriteICB(VkDrawIndexedIndirectCommand *indexed_draw_cmd, DrawBatch *batch)
{
    indexed_draw_cmd->indexCount = batch->geom_draw_range->index_count;
    indexed_draw_cmd->instanceCount = batch->instance_count;
    indexed_draw_cmd->firstIndex = batch->geom_draw_range->first_index;
    indexed_draw_cmd->vertexOffset = batch->geom_draw_range->vertex_offset;
    indexed_draw_cmd->firstInstance = batch->first_instance;
}

void PipelineMaterialBatch::BuildBatches()
{
    /**
     * 批次构建算法：
     * 1. 遍历已排序的渲染节点
     * 2. 合并使用相同几何数据和绘制范围的节点到同一批次
     * 3. 为每个批次生成间接绘制命令
     * 4. 支持带索引和不带索引的绘制
     */
    
    const uint count = draw_nodes.GetCount();
    DrawNode **node_ptr = draw_nodes.GetData();

    // 准备间接绘制缓冲
    ReallocICB();

    VkDrawIndirectCommand *draw_cmd = icb_draw->MapCmd();
    VkDrawIndexedIndirectCommand *indexed_draw_cmd = icb_draw_indexed->MapCmd();

    // 准备批次数组
    draw_batches.Clear();
    draw_batches.Reserve(count);

    // 初始化第一个批次
    DrawBatch *batch = draw_batches.GetData();
    Primitive *primitive = (*node_ptr)->GetPrimitive();

    draw_batches_count = 1;

    batch->first_instance = 0;
    batch->instance_count = 1;
    batch->Set(primitive);

    // 缓存当前批次的几何信息（用于批次合并判断）
    const GeometryDataBuffer *last_data_buffer = batch->geom_data_buffer;
    const GeometryDrawRange *last_draw_range = batch->geom_draw_range;

    ++node_ptr;

    // 处理剩余节点
    for (uint i = 1; i < count; i++)
    {
        primitive = (*node_ptr)->GetPrimitive();

        // 检查是否可以合并到当前批次
        if (*last_data_buffer == *primitive->GetDataBuffer() &&
            *last_draw_range == *primitive->GetRenderData())
        {
            ++batch->instance_count;
            ++node_ptr;
            continue;
        }

        // 完成当前批次的间接绘制命令
        if (batch->geom_data_buffer->vdm)
        {
            if (batch->geom_data_buffer->ibo)
                WriteICB(indexed_draw_cmd++, batch);
            else
                WriteICB(draw_cmd++, batch);
        }

        // 开始新批次
        ++draw_batches_count;
        ++batch;

        batch->first_instance = i;
        batch->instance_count = 1;
        batch->Set(primitive);

        // 更新缓存
        last_data_buffer = batch->geom_data_buffer;
        last_draw_range = batch->geom_draw_range;

        ++node_ptr;
    }

    // 完成最后一个批次
    if (batch->geom_data_buffer->vdm)
    {
        if (batch->geom_data_buffer->ibo)
            WriteICB(indexed_draw_cmd, batch);
        else
            WriteICB(draw_cmd, batch);
    }

    icb_draw->Unmap();
    icb_draw_indexed->Unmap();
}

void PipelineMaterialBatch::Render(RenderCmdBuffer *rcb)
{
    // 前置条件检查
    if (!rcb) return;
    
    const uint count = draw_nodes.GetCount();
    if (count <= 0) return;

    if (draw_batches_count <= 0) return;

    // 委托给渲染器执行渲染
    renderer->Render(rcb, draw_batches, draw_batches_count, 
                    assign_buffer, icb_draw, icb_draw_indexed);
}

VK_NAMESPACE_END
