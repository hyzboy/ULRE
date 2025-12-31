#include<hgl/graph/PipelineMaterialBatch.h>
#include<hgl/graph/PipelineMaterialRenderer.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKVertexInput.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/util/sort/Sort.h>
#include"TransformAssignmentBuffer.h"
#include"MaterialInstanceAssignmentBuffer.h"
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/component/PrimitiveComponent.h>
#include<iostream>

VK_NAMESPACE_BEGIN

PipelineMaterialBatch::PipelineMaterialBatch(VulkanDevice *d, bool l2w, const PipelineMaterialIndex &rpi)
    : device(d)
    , pm_index(rpi)
    , camera_info(nullptr)
    , transform_buffer(nullptr)
    , mi_buffer(nullptr)
    , draw_batches_count(0)
    , icb_draw(nullptr)
    , icb_draw_indexed(nullptr)
    , renderer(nullptr)
{
    std::cout << "[PipelineMaterialBatch] Constructor - Material: " << (void*)rpi.material 
              << ", Pipeline: " << (void*)rpi.pipeline << std::endl;
    std::cout << "[PipelineMaterialBatch] Material hasLocalToWorld: " << (rpi.material->hasLocalToWorld() ? "YES" : "NO") << std::endl;
    std::cout << "[PipelineMaterialBatch] Material hasMI: " << (rpi.material->hasMI() ? "YES" : "NO") << std::endl;
    
    // 如果材质需要 LocalToWorld，创建Transform分配缓冲
    if (rpi.material->hasLocalToWorld())
    {
        std::cout << "[PipelineMaterialBatch] Creating TransformAssignmentBuffer..." << std::endl;
        transform_buffer = new TransformAssignmentBuffer(device);
        std::cout << "[PipelineMaterialBatch] TransformAssignmentBuffer created: " << (void*)transform_buffer << std::endl;
    }

    // 如果材质需要材质实例数据，创建MI分配缓冲
    if (rpi.material->hasMI())
    {
        std::cout << "[PipelineMaterialBatch] Creating MaterialInstanceAssignmentBuffer..." << std::endl;
        mi_buffer = new MaterialInstanceAssignmentBuffer(device, pm_index.material);
        std::cout << "[PipelineMaterialBatch] MaterialInstanceAssignmentBuffer created: " << (void*)mi_buffer << std::endl;
    }

    // 创建渲染器
    std::cout << "[PipelineMaterialBatch] Creating PipelineMaterialRenderer..." << std::endl;
    renderer = new PipelineMaterialRenderer(pm_index.material, pm_index.pipeline);
    std::cout << "[PipelineMaterialBatch] PipelineMaterialRenderer created: " << (void*)renderer << std::endl;
}

PipelineMaterialBatch::~PipelineMaterialBatch()
{
    SAFE_CLEAR(icb_draw_indexed)
    SAFE_CLEAR(icb_draw)
    SAFE_CLEAR(transform_buffer);
    SAFE_CLEAR(mi_buffer);
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
        node->to_camera_distance = math::Length(camera_info->pos, node->world_position);
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
    std::cout << "[PipelineMaterialBatch::Finalize] === Starting finalization ===" << std::endl;
    std::cout << "[PipelineMaterialBatch::Finalize] Node count: " << draw_nodes.GetCount() << std::endl;
    
    // 排序节点以优化渲染顺序
    Sort(draw_nodes.GetArray());

    const uint node_count = draw_nodes.GetCount();
    if (node_count <= 0)
    {
        std::cout << "[PipelineMaterialBatch::Finalize] No nodes to finalize" << std::endl;
        return;
    }

    std::cout << "[PipelineMaterialBatch::Finalize] Building batches..." << std::endl;
    // 构建绘制批次
    BuildBatches();
    std::cout << "[PipelineMaterialBatch::Finalize] Batches built, count: " << draw_batches_count << std::endl;

    // 写入实例数据到缓冲
    if (transform_buffer)
    {
        std::cout << "[PipelineMaterialBatch::Finalize] Writing Transform data to buffer..." << std::endl;
        transform_buffer->WriteNode(draw_nodes);
        std::cout << "[PipelineMaterialBatch::Finalize] Transform data written" << std::endl;
    }
    else
    {
        std::cout << "[PipelineMaterialBatch::Finalize] No Transform buffer to write" << std::endl;
    }
    
    if (mi_buffer)
    {
        std::cout << "[PipelineMaterialBatch::Finalize] Writing MaterialInstance data to buffer..." << std::endl;
        mi_buffer->WriteNode(draw_nodes);
        std::cout << "[PipelineMaterialBatch::Finalize] MaterialInstance data written" << std::endl;
    }
    else
    {
        std::cout << "[PipelineMaterialBatch::Finalize] No MaterialInstance buffer to write" << std::endl;
    }
    
    std::cout << "[PipelineMaterialBatch::Finalize] === Finalization complete ===" << std::endl;
}

void PipelineMaterialBatch::UpdateTransformData()
{
    if (!transform_buffer) return;

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
        transform_buffer->UpdateTransformData(transform_dirty_nodes, first_index, last_index);
        transform_dirty_nodes.Clear();
    }
}

void PipelineMaterialBatch::UpdateMaterialInstanceData(PrimitiveComponent *prim_component)
{
    // 提前返回，减少嵌套
    if (!prim_component) return;
    if (!mi_buffer) return;
    
    const int node_count = draw_nodes.GetCount();
    if (node_count <= 0) return;

    DrawNode **node_ptr = draw_nodes.GetData();
    for (int i = 0; i < node_count; i++, node_ptr++)
    {
        DrawNode *node = *node_ptr;

        if (node->GetPrimitiveComponent() == prim_component)
        {
            mi_buffer->UpdateMaterialInstanceData(node);
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

    // 用于批次合并判断的缓存变量
    const GeometryDataBuffer *current_data_buffer = batch->geom_data_buffer;
    const GeometryDrawRange *current_draw_range = batch->geom_draw_range;

    ++node_ptr;

    // 处理剩余节点
    for (uint i = 1; i < count; i++)
    {
        primitive = (*node_ptr)->GetPrimitive();

        // 检查是否可以合并到当前批次
        if (*current_data_buffer == *primitive->GetDataBuffer() &&
            *current_draw_range == *primitive->GetRenderData())
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
        current_data_buffer = batch->geom_data_buffer;
        current_draw_range = batch->geom_draw_range;

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
                    transform_buffer, mi_buffer, icb_draw, icb_draw_indexed);
}

VK_NAMESPACE_END
