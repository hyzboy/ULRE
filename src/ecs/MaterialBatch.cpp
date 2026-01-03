#include<hgl/ecs/MaterialBatch.h>
#include<hgl/ecs/RenderItem.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>
#include"ECSPipelineMaterialRenderer.h"
#include<algorithm>
#include<iostream>

// Import ECS-specific assignment buffers
#include"ECSTransformAssignmentBuffer.h"
#include"ECSMaterialInstanceAssignmentBuffer.h"

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
            // Create ECS renderer
            renderer = new ECSPipelineMaterialRenderer(key.material, key.pipeline);

            // Create ECSTransformAssignmentBuffer if material needs LocalToWorld
            if (key.material->hasLocalToWorld())
            {
                transform_buffer = new ECSTransformAssignmentBuffer(device);
            }

            // Create ECSMaterialInstanceAssignmentBuffer if material has material instance data
            if (key.material->hasMI())
            {
                mi_buffer = new ECSMaterialInstanceAssignmentBuffer(device, key.material);
            }
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

        // Write transform data to buffer
        if (transform_buffer && !items.empty())
        {
            transform_buffer->WriteItems(items);
        }

        // Write material instance data to buffer
        if (mi_buffer && !items.empty())
        {
            mi_buffer->WriteItems(items);
        }
    }

    void MaterialBatch::ReallocICB()
    {
        if (!device || items.empty())
        {
            std::cout << "[ECS::MaterialBatch::ReallocICB] Cannot allocate - Device: " 
                      << (void*)device << ", Items: " << items.size() << std::endl;
            return;
        }

        // Calculate required buffer size (power of 2)
        uint32_t icb_new_count = 1;
        while (icb_new_count < items.size())
            icb_new_count <<= 1;

        // If existing buffers are large enough, reuse them
        if (icb_draw && icb_new_count <= icb_draw->GetMaxCount())
        {
            std::cout << "[ECS::MaterialBatch::ReallocICB] Reusing existing buffers (capacity: " 
                      << icb_draw->GetMaxCount() << ")" << std::endl;
            return;
        }

        // Delete old buffers
        if (icb_draw)
        {
            std::cout << "[ECS::MaterialBatch::ReallocICB] Deleting old indirect draw buffer" << std::endl;
            delete icb_draw;
        }
        if (icb_draw_indexed)
        {
            std::cout << "[ECS::MaterialBatch::ReallocICB] Deleting old indexed indirect draw buffer" << std::endl;
            delete icb_draw_indexed;
        }

        // Create new buffers
        icb_draw = device->CreateIndirectDrawBuffer(icb_new_count);
        icb_draw_indexed = device->CreateIndirectDrawIndexedBuffer(icb_new_count);
    }

    void MaterialBatch::WriteICB(VkDrawIndirectCommand* draw_cmd, graph::DrawBatch* batch)
    {
        if (!draw_cmd || !batch || !batch->geom_draw_range)
        {
            std::cout << "[ECS::MaterialBatch::WriteICB] ERROR: Invalid parameters - DrawCmd: " 
                      << (void*)draw_cmd << ", Batch: " << (void*)batch << std::endl;
            return;
        }

        draw_cmd->vertexCount = batch->geom_draw_range->vertex_count;
        draw_cmd->instanceCount = batch->instance_count;
        draw_cmd->firstVertex = batch->geom_draw_range->vertex_offset;
        draw_cmd->firstInstance = batch->first_instance;
    }

    void MaterialBatch::WriteICB(VkDrawIndexedIndirectCommand* indexed_draw_cmd, graph::DrawBatch* batch)
    {
        if (!indexed_draw_cmd || !batch || !batch->geom_draw_range)
        {
            std::cout << "[ECS::MaterialBatch::WriteICB] ERROR: Invalid parameters - IndexedDrawCmd: " 
                      << (void*)indexed_draw_cmd << ", Batch: " << (void*)batch << std::endl;
            return;
        }

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
            std::cout << "[ECS::MaterialBatch::BuildBatches] No items to batch" << std::endl;
            draw_batches_count = 0;
            return;
        }

        // Allocate indirect command buffers
        ReallocICB();

        if (!icb_draw || !icb_draw_indexed)
        {
            std::cout << "[ECS::MaterialBatch::BuildBatches] ERROR: Failed to allocate indirect buffers!" << std::endl;
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
            std::cout << "[ECS::MaterialBatch::BuildBatches] ERROR: First item has no primitive!" << std::endl;
            icb_draw->Unmap();
            icb_draw_indexed->Unmap();
            draw_batches_count = 0;
            return;
        }

        draw_batches_count = 1;
        batch->first_instance = 0;
        batch->instance_count = 1;
        batch->Set(primitive);

        const graph::GeometryDataBuffer* data_buf = batch->geom_data_buffer;
        const graph::GeometryDrawRange* draw_range = batch->geom_draw_range;

        // Cache for batch merging
        const graph::GeometryDataBuffer* current_data_buffer = batch->geom_data_buffer;
        const graph::GeometryDrawRange* current_draw_range = batch->geom_draw_range;

        // Process remaining items
        for (size_t i = 1; i < count; i++)
        {
            item = items[i];
            primitive = item->GetPrimitive();

            if (!primitive)
            {
                std::cout << "[ECS::MaterialBatch::BuildBatches] WARNING: Item " << i << " has no primitive, skipping" << std::endl;
                continue;
            }

            const graph::GeometryDataBuffer* item_data_buf = primitive->GetDataBuffer();
            const graph::GeometryDrawRange* item_draw_range = primitive->GetRenderData();

            // Check if we can merge with current batch
            if (*current_data_buffer == *item_data_buf &&
                *current_draw_range == *item_draw_range)
            {
                ++batch->instance_count;
                continue;
            }

            // Write indirect command for completed batch
            if (batch->geom_data_buffer && batch->geom_data_buffer->vdm)
            {
                if (batch->geom_data_buffer->ibo)
                {
                    WriteICB(indexed_draw_cmd++, batch);
                }
                else
                {
                    WriteICB(draw_cmd++, batch);
                }
            }
            // else
            // {
            //     std::cout << "[ECS::MaterialBatch::BuildBatches] Batch " << (draw_batches_count - 1) 
            //               << " has NO VDM - will use direct rendering" << std::endl;
            // }

            // Start new batch
            ++draw_batches_count;
            ++batch;

            batch->first_instance = i;
            batch->instance_count = 1;
            batch->Set(primitive);

            data_buf = batch->geom_data_buffer;
            draw_range = batch->geom_draw_range;

            // Update cache
            current_data_buffer = batch->geom_data_buffer;
            current_draw_range = batch->geom_draw_range;
        }

        // Write indirect command for last batch
        if (batch->geom_data_buffer && batch->geom_data_buffer->vdm)
        {
            if (batch->geom_data_buffer->ibo)
            {
                WriteICB(indexed_draw_cmd, batch);
            }
            else
            {
                WriteICB(draw_cmd, batch);
            }
        }
        // else
        // {
        //     std::cout << "[ECS::MaterialBatch::BuildBatches] LAST batch " << (draw_batches_count - 1) 
        //               << " has NO VDM - will use direct rendering" << std::endl;
        // }

        icb_draw->Unmap();
        icb_draw_indexed->Unmap();
    }

    void MaterialBatch::Render(graph::RenderCmdBuffer* cmdBuffer)
    {
        if (!cmdBuffer || items.empty())
        {
            std::cout << "[ECS::MaterialBatch::Render] ERROR: Invalid cmdBuffer or no items!" << std::endl;
            return;
        }

        if (!key.material || !key.pipeline)
        {
            std::cout << "[ECS::MaterialBatch::Render] ERROR: No material or pipeline! Material: " 
                      << (void*)key.material << ", Pipeline: " << (void*)key.pipeline << std::endl;
            return;
        }

        if (draw_batches_count == 0)
        {
            std::cout << "[ECS::MaterialBatch::Render] ERROR: No batches to render!" << std::endl;
            return;
        }

        // Bind transform data if available
        if (transform_buffer)
        {
            std::cout << "[ECS::MaterialBatch::Render] Binding transform buffer..." << std::endl;
            transform_buffer->BindTransform(key.material);
        }

        // Bind material instance data if available
        if (mi_buffer)
        {
            std::cout << "[ECS::MaterialBatch::Render] Binding material instance buffer..." << std::endl;
            mi_buffer->BindMaterialInstance(key.material);
        }

        // Use the ECS renderer to handle rendering with ECS assignment buffers
        if (renderer)
        {
            // Pass ECS buffers directly to ECS renderer
            renderer->Render(cmdBuffer, draw_batches, draw_batches_count,
                           transform_buffer, mi_buffer, icb_draw, icb_draw_indexed);
        }
        // else
        // {
        //     std::cout << "[ECS::MaterialBatch::Render] ERROR: No renderer available!" << std::endl;
        // }
    }
}//namespace hgl::ecs
