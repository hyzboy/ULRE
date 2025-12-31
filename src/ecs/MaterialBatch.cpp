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
#include<iostream>

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
        std::cout << "[ECS::MaterialBatch] Constructor - Material: " << (void*)key.material 
                  << ", Pipeline: " << (void*)key.pipeline << std::endl;
        
        if (key.material && key.pipeline)
        {
            // Create renderer
            renderer = new graph::PipelineMaterialRenderer(key.material, key.pipeline);
            std::cout << "[ECS::MaterialBatch] Created renderer: " << (void*)renderer << std::endl;
            
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
        std::cout << "[ECS::MaterialBatch] Destructor - Cleaning up batch for Material: " 
                  << (void*)key.material << std::endl;
        
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
            std::cout << "[ECS::MaterialBatch::AddItem] Added item " << item->index 
                      << ", Primitive: " << (void*)item->GetPrimitive() 
                      << ", Total items: " << items.size() << std::endl;
        }
    }

    void MaterialBatch::Finalize()
    {
        std::cout << "[ECS::MaterialBatch::Finalize] Starting finalization with " 
                  << items.size() << " items" << std::endl;
        
        // Sort items by geometry/distance for optimal rendering
        std::sort(items.begin(), items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                return a->Compare(*b) < 0;
            });

        std::cout << "[ECS::MaterialBatch::Finalize] Items sorted, building batches..." << std::endl;
        
        // Build batches and indirect draw commands
        BuildBatches();
        
        std::cout << "[ECS::MaterialBatch::Finalize] Finalization complete. Batch count: " 
                  << draw_batches_count << std::endl;
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

        std::cout << "[ECS::MaterialBatch::ReallocICB] Need " << items.size() 
                  << " items, allocating " << icb_new_count << " buffer slots" << std::endl;

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
        std::cout << "[ECS::MaterialBatch::ReallocICB] Creating new indirect buffers with capacity " 
                  << icb_new_count << std::endl;
        icb_draw = device->CreateIndirectDrawBuffer(icb_new_count);
        icb_draw_indexed = device->CreateIndirectDrawIndexedBuffer(icb_new_count);
        
        std::cout << "[ECS::MaterialBatch::ReallocICB] Buffers created - Draw: " << (void*)icb_draw 
                  << ", Indexed: " << (void*)icb_draw_indexed << std::endl;
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
        
        std::cout << "[ECS::MaterialBatch::WriteICB] Non-indexed command: "
                  << "vertexCount=" << draw_cmd->vertexCount
                  << ", instanceCount=" << draw_cmd->instanceCount
                  << ", firstVertex=" << draw_cmd->firstVertex
                  << ", firstInstance=" << draw_cmd->firstInstance << std::endl;
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
        
        std::cout << "[ECS::MaterialBatch::WriteICB] Indexed command: "
                  << "indexCount=" << indexed_draw_cmd->indexCount
                  << ", instanceCount=" << indexed_draw_cmd->instanceCount
                  << ", firstIndex=" << indexed_draw_cmd->firstIndex
                  << ", vertexOffset=" << indexed_draw_cmd->vertexOffset
                  << ", firstInstance=" << indexed_draw_cmd->firstInstance << std::endl;
    }

    void MaterialBatch::BuildBatches()
    {
        std::cout << "[ECS::MaterialBatch::BuildBatches] === Starting batch building ===" << std::endl;
        
        const size_t count = items.size();
        if (count == 0)
        {
            std::cout << "[ECS::MaterialBatch::BuildBatches] No items to batch" << std::endl;
            draw_batches_count = 0;
            return;
        }

        std::cout << "[ECS::MaterialBatch::BuildBatches] Processing " << count << " items" << std::endl;

        // Allocate indirect command buffers
        ReallocICB();
        
        if (!icb_draw || !icb_draw_indexed)
        {
            std::cout << "[ECS::MaterialBatch::BuildBatches] ERROR: Failed to allocate indirect buffers!" << std::endl;
            draw_batches_count = 0;
            return;
        }

        // Map indirect command buffers
        std::cout << "[ECS::MaterialBatch::BuildBatches] Mapping indirect command buffers..." << std::endl;
        VkDrawIndirectCommand* draw_cmd = icb_draw->MapCmd();
        VkDrawIndexedIndirectCommand* indexed_draw_cmd = icb_draw_indexed->MapCmd();
        std::cout << "[ECS::MaterialBatch::BuildBatches] Buffers mapped - Draw: " << (void*)draw_cmd 
                  << ", Indexed: " << (void*)indexed_draw_cmd << std::endl;

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

        std::cout << "[ECS::MaterialBatch::BuildBatches] First item - Primitive: " << (void*)primitive << std::endl;

        draw_batches_count = 1;
        batch->first_instance = 0;
        batch->instance_count = 1;
        batch->Set(primitive);

        const graph::GeometryDataBuffer* data_buf = batch->geom_data_buffer;
        const graph::GeometryDrawRange* draw_range = batch->geom_draw_range;
        
        std::cout << "[ECS::MaterialBatch::BuildBatches] Batch 0: DataBuffer=" << (void*)data_buf 
                  << ", DrawRange=" << (void*)draw_range 
                  << ", VDM=" << (void*)(data_buf ? data_buf->vdm : nullptr)
                  << ", IBO=" << (void*)(data_buf && data_buf->ibo ? data_buf->ibo : nullptr) << std::endl;

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
                std::cout << "[ECS::MaterialBatch::BuildBatches] Item " << i << " merged into batch " 
                          << (draw_batches_count - 1) << " (instance count: " << batch->instance_count << ")" << std::endl;
                continue;
            }

            std::cout << "[ECS::MaterialBatch::BuildBatches] Item " << i << " starts new batch (geometry mismatch)" << std::endl;

            // Write indirect command for completed batch
            if (batch->geom_data_buffer && batch->geom_data_buffer->vdm)
            {
                std::cout << "[ECS::MaterialBatch::BuildBatches] Writing indirect command for batch " 
                          << (draw_batches_count - 1) << " (instances: " << batch->instance_count << ")" << std::endl;
                if (batch->geom_data_buffer->ibo)
                {
                    std::cout << "[ECS::MaterialBatch::BuildBatches]   -> Indexed draw" << std::endl;
                    WriteICB(indexed_draw_cmd++, batch);
                }
                else
                {
                    std::cout << "[ECS::MaterialBatch::BuildBatches]   -> Non-indexed draw" << std::endl;
                    WriteICB(draw_cmd++, batch);
                }
            }
            else
            {
                std::cout << "[ECS::MaterialBatch::BuildBatches] Batch " << (draw_batches_count - 1) 
                          << " has NO VDM - will use direct rendering" << std::endl;
            }

            // Start new batch
            ++draw_batches_count;
            ++batch;

            batch->first_instance = i;
            batch->instance_count = 1;
            batch->Set(primitive);

            data_buf = batch->geom_data_buffer;
            draw_range = batch->geom_draw_range;
            
            std::cout << "[ECS::MaterialBatch::BuildBatches] Batch " << (draw_batches_count - 1) 
                      << ": DataBuffer=" << (void*)data_buf 
                      << ", DrawRange=" << (void*)draw_range 
                      << ", VDM=" << (void*)(data_buf ? data_buf->vdm : nullptr)
                      << ", IBO=" << (void*)(data_buf && data_buf->ibo ? data_buf->ibo : nullptr) << std::endl;

            // Update cache
            current_data_buffer = batch->geom_data_buffer;
            current_draw_range = batch->geom_draw_range;
        }

        // Write indirect command for last batch
        if (batch->geom_data_buffer && batch->geom_data_buffer->vdm)
        {
            std::cout << "[ECS::MaterialBatch::BuildBatches] Writing indirect command for LAST batch " 
                      << (draw_batches_count - 1) << " (instances: " << batch->instance_count << ")" << std::endl;
            if (batch->geom_data_buffer->ibo)
            {
                std::cout << "[ECS::MaterialBatch::BuildBatches]   -> Indexed draw" << std::endl;
                WriteICB(indexed_draw_cmd, batch);
            }
            else
            {
                std::cout << "[ECS::MaterialBatch::BuildBatches]   -> Non-indexed draw" << std::endl;
                WriteICB(draw_cmd, batch);
            }
        }
        else
        {
            std::cout << "[ECS::MaterialBatch::BuildBatches] LAST batch " << (draw_batches_count - 1) 
                      << " has NO VDM - will use direct rendering" << std::endl;
        }

        icb_draw->Unmap();
        icb_draw_indexed->Unmap();
        
        std::cout << "[ECS::MaterialBatch::BuildBatches] === Batch building complete ===" << std::endl;
        std::cout << "[ECS::MaterialBatch::BuildBatches] Total batches created: " << draw_batches_count << std::endl;
    }

    void MaterialBatch::Render(graph::RenderCmdBuffer* cmdBuffer)
    {
        std::cout << "[ECS::MaterialBatch::Render] === Starting render ===" << std::endl;
        std::cout << "[ECS::MaterialBatch::Render] CmdBuffer: " << (void*)cmdBuffer 
                  << ", Items: " << items.size() 
                  << ", Batches: " << draw_batches_count << std::endl;
        
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

        // Use the renderer to handle indirect rendering
        if (renderer)
        {
            std::cout << "[ECS::MaterialBatch::Render] Delegating to PipelineMaterialRenderer..." << std::endl;
            std::cout << "[ECS::MaterialBatch::Render] Params:" << std::endl;
            std::cout << "  - Batches: " << draw_batches_count << std::endl;
            std::cout << "  - TransformBuffer: " << (void*)transform_buffer << std::endl;
            std::cout << "  - MaterialInstanceBuffer: " << (void*)mi_buffer << std::endl;
            std::cout << "  - IndirectDrawBuffer: " << (void*)icb_draw << std::endl;
            std::cout << "  - IndirectDrawIndexedBuffer: " << (void*)icb_draw_indexed << std::endl;
            
            renderer->Render(cmdBuffer, draw_batches, draw_batches_count,
                           transform_buffer, mi_buffer, icb_draw, icb_draw_indexed);
            
            std::cout << "[ECS::MaterialBatch::Render] === Render complete ===" << std::endl;
        }
        else
        {
            std::cout << "[ECS::MaterialBatch::Render] ERROR: No renderer available!" << std::endl;
        }
    }
}//namespace hgl::ecs
