#include<hgl/ecs/MaterialBatch.h>
#include<hgl/ecs/RenderItem.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>
#include<hgl/graph/PipelineMaterialRenderer.h>
#include<algorithm>

// Import TransformAssignmentBuffer and MaterialInstanceAssignmentBuffer from SceneGraph
// These are internal headers in src/SceneGraph/render
namespace hgl::graph
{
    class TransformAssignmentBuffer;
    class MaterialInstanceAssignmentBuffer;
}

namespace hgl::ecs
{
    MaterialBatch::MaterialBatch(const MaterialPipelineKey& k, graph::VulkanDevice* dev)
        : key(k)
        , cameraInfo(nullptr)
        , device(dev)
        , icb_draw(nullptr)
        , icb_draw_indexed(nullptr)
        , transform_buffer(nullptr)
        , mi_buffer(nullptr)
        , draw_batches_count(0)
        , renderer(nullptr)
    {
        if (key.material && key.pipeline)
        {
            // Create renderer
            renderer = new graph::PipelineMaterialRenderer(key.material, key.pipeline);
            
            // Note: TransformAssignmentBuffer and MaterialInstanceAssignmentBuffer
            // are not created here because they require access to internal headers
            // from src/SceneGraph/render. For now, we'll use nullptr and the renderer
            // will handle the direct rendering path.
            // 
            // In a full implementation, these would be:
            // if (key.material->hasLocalToWorld())
            //     transform_buffer = new TransformAssignmentBuffer(device);
            // if (key.material->hasMI())
            //     mi_buffer = new MaterialInstanceAssignmentBuffer(device, key.material);
        }
    }

    MaterialBatch::~MaterialBatch()
    {
        if (icb_draw_indexed)
            delete icb_draw_indexed;
        if (icb_draw)
            delete icb_draw;
        if (transform_buffer)
            delete transform_buffer;
        if (mi_buffer)
            delete mi_buffer;
        if (renderer)
            delete renderer;
    }

    void MaterialBatch::AddItem(RenderItem* item)
    {
        if (item)
        {
            item->index = items.size();
            items.push_back(item);
        }
    }

    void MaterialBatch::Finalize()
    {
        // Sort items by geometry/distance for optimal rendering
        std::sort(items.begin(), items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                return a->Compare(*b) < 0;
            });

        // Build batches and indirect draw commands
        BuildBatches();
    }

    void MaterialBatch::ReallocICB()
    {
        if (!device || items.empty())
            return;

        // Calculate required buffer size (power of 2)
        uint32_t icb_new_count = 1;
        while (icb_new_count < items.size())
            icb_new_count <<= 1;

        // If existing buffers are large enough, reuse them
        if (icb_draw && icb_new_count <= icb_draw->GetMaxCount())
            return;

        // Delete old buffers
        if (icb_draw)
            delete icb_draw;
        if (icb_draw_indexed)
            delete icb_draw_indexed;

        // Create new buffers
        icb_draw = device->CreateIndirectDrawBuffer(icb_new_count);
        icb_draw_indexed = device->CreateIndirectDrawIndexedBuffer(icb_new_count);
    }

    void MaterialBatch::WriteICB(VkDrawIndirectCommand* draw_cmd, graph::DrawBatch* batch)
    {
        if (!draw_cmd || !batch || !batch->geom_draw_range)
            return;

        draw_cmd->vertexCount = batch->geom_draw_range->vertex_count;
        draw_cmd->instanceCount = batch->instance_count;
        draw_cmd->firstVertex = batch->geom_draw_range->vertex_offset;
        draw_cmd->firstInstance = batch->first_instance;
    }

    void MaterialBatch::WriteICB(VkDrawIndexedIndirectCommand* indexed_draw_cmd, graph::DrawBatch* batch)
    {
        if (!indexed_draw_cmd || !batch || !batch->geom_draw_range)
            return;

        indexed_draw_cmd->indexCount = batch->geom_draw_range->index_count;
        indexed_draw_cmd->instanceCount = batch->instance_count;
        indexed_draw_cmd->firstIndex = batch->geom_draw_range->first_index;
        indexed_draw_cmd->vertexOffset = batch->geom_draw_range->vertex_offset;
        indexed_draw_cmd->firstInstance = batch->first_instance;
    }

    void MaterialBatch::BuildBatches()
    {
        const size_t count = items.size();
        if (count == 0)
        {
            draw_batches_count = 0;
            return;
        }

        // Allocate indirect command buffers
        ReallocICB();
        
        if (!icb_draw || !icb_draw_indexed)
        {
            draw_batches_count = 0;
            return;
        }

        // Map indirect command buffers
        VkDrawIndirectCommand* draw_cmd = icb_draw->MapCmd();
        VkDrawIndexedIndirectCommand* indexed_draw_cmd = icb_draw_indexed->MapCmd();

        // Prepare batch array
        draw_batches.Clear();
        draw_batches.Reserve(count);

        // Initialize first batch
        graph::DrawBatch* batch = draw_batches.GetData();
        RenderItem* item = items[0];
        graph::Primitive* primitive = item->GetPrimitive();
        
        if (!primitive)
        {
            icb_draw->Unmap();
            icb_draw_indexed->Unmap();
            draw_batches_count = 0;
            return;
        }

        draw_batches_count = 1;
        batch->first_instance = 0;
        batch->instance_count = 1;
        batch->Set(primitive);

        // Cache for batch merging
        const graph::GeometryDataBuffer* current_data_buffer = batch->geom_data_buffer;
        const graph::GeometryDrawRange* current_draw_range = batch->geom_draw_range;

        // Process remaining items
        for (size_t i = 1; i < count; i++)
        {
            item = items[i];
            primitive = item->GetPrimitive();
            
            if (!primitive)
                continue;

            // Check if we can merge with current batch
            if (*current_data_buffer == *primitive->GetDataBuffer() &&
                *current_draw_range == *primitive->GetRenderData())
            {
                ++batch->instance_count;
                continue;
            }

            // Write indirect command for completed batch
            if (batch->geom_data_buffer && batch->geom_data_buffer->vdm)
            {
                if (batch->geom_data_buffer->ibo)
                    WriteICB(indexed_draw_cmd++, batch);
                else
                    WriteICB(draw_cmd++, batch);
            }

            // Start new batch
            ++draw_batches_count;
            ++batch;

            batch->first_instance = i;
            batch->instance_count = 1;
            batch->Set(primitive);

            // Update cache
            current_data_buffer = batch->geom_data_buffer;
            current_draw_range = batch->geom_draw_range;
        }

        // Write indirect command for last batch
        if (batch->geom_data_buffer && batch->geom_data_buffer->vdm)
        {
            if (batch->geom_data_buffer->ibo)
                WriteICB(indexed_draw_cmd, batch);
            else
                WriteICB(draw_cmd, batch);
        }

        icb_draw->Unmap();
        icb_draw_indexed->Unmap();
    }

    void MaterialBatch::Render(graph::RenderCmdBuffer* cmdBuffer)
    {
        if (!cmdBuffer || items.empty())
            return;

        if (!key.material || !key.pipeline)
            return;

        if (draw_batches_count == 0)
            return;

        // Use the renderer to handle indirect rendering
        if (renderer)
        {
            renderer->Render(cmdBuffer, draw_batches, draw_batches_count,
                           transform_buffer, mi_buffer, icb_draw, icb_draw_indexed);
        }
    }
}//namespace hgl::ecs
