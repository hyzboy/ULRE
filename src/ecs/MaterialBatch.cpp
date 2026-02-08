#include<hgl/ecs/MaterialBatch.h>
#include<hgl/ecs/RenderItem.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include"ECSPipelineMaterialRenderer.h"
#include<hgl/ecs/TransformComponent.h>
#include<algorithm>
#include<iostream>
#include<limits>

// Import ECS-specific assignment buffers
#include"ECSTransformAssignmentBuffer.h"
#include"ECSMaterialInstanceAssignmentBuffer.h"

namespace hgl::ecs
{
    MaterialBatch::MaterialBatch(const MaterialPipelineKey& k, graph::VulkanDevice* dev)
        : key(k)
        , static_count(0)
        , cameraInfo(nullptr)
        , device(dev)
        , draw_batches_count(0)
        , renderer(nullptr)
    {
        if (key.material && key.pipeline)
        {
            // Create ECS renderer
            renderer = new ECSPipelineMaterialRenderer(key.material, key.pipeline);
        }
    }

    MaterialBatch::~MaterialBatch()
    {
        if (icb_draw_indexed)
            delete icb_draw_indexed;
        if (icb_draw)
            delete icb_draw;
        if (transform_vab)
            delete transform_vab;
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
        std::vector<RenderItem*> static_items;
        std::vector<RenderItem*> movable_items;
        static_items.reserve(items.size());
        movable_items.reserve(items.size());

        for (auto *item : items)
        {
            if (!item)
                continue;

            auto transform = item->GetTransform();
            if (transform && !transform->IsMovable())
                static_items.push_back(item);
            else
                movable_items.push_back(item);
        }

        std::sort(static_items.begin(), static_items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                return a->Compare(*b) < 0;
            });

        std::sort(movable_items.begin(), movable_items.end(),
            [](const RenderItem* a, const RenderItem* b) {
                return a->Compare(*b) < 0;
            });

        items.clear();
        items.reserve(static_items.size() + movable_items.size());
        for (auto *item : static_items)
            items.push_back(item);
        for (auto *item : movable_items)
            items.push_back(item);

        for (size_t i = 0; i < items.size(); ++i)
        {
            items[i]->index = i;
        }

        static_count = static_cast<uint32_t>(static_items.size());

        // Build batches and indirect draw commands
        BuildBatches(items, draw_batches, draw_batches_count,
                     icb_draw, icb_draw_indexed, 0);

        // Write material instance data to buffer
        if (key.material && key.material->hasMI())
        {
            if (!mi_buffer && !items.empty())
                mi_buffer = new ECSMaterialInstanceAssignmentBuffer(device, key.material);

            if (mi_buffer && !items.empty())
                mi_buffer->WriteItems(items);
        }

        if (device && !items.empty())
        {
        #ifdef _DEBUG
            if (transform_buffer)
            {
                uint32_t max_transform_index = 0;
                for (auto *item : items)
                {
                    if (item && item->transform_index > max_transform_index)
                        max_transform_index = item->transform_index;
                }

                auto static_storage = TransformComponent::GetStaticStorage();
                auto dynamic_storage = TransformComponent::GetDynamicStorage();
                const uint32_t static_count = static_cast<uint32_t>(static_storage ? static_storage->GetSize() : 0);
                const uint32_t dynamic_count = static_cast<uint32_t>(dynamic_storage ? dynamic_storage->GetSize() : 0);
                const uint32_t total_count = transform_buffer->GetTotalCount(static_count, dynamic_count);

                if (total_count > 0 && max_transform_index >= total_count)
                {
                    std::cout << "[ECS::MaterialBatch::Finalize] WARNING: Transform index out of range ("
                              << max_transform_index << " >= " << total_count << ")" << std::endl;
                }
            }
        #endif

            const uint32_t item_count = static_cast<uint32_t>(items.size());
            uint32_t new_node_count = 1;
            while (new_node_count < item_count)
                new_node_count <<= 1;

            if (!transform_vab || transform_vab_node_count < item_count)
            {
                transform_vab_node_count = new_node_count;

                if (transform_vab)
                    delete transform_vab;

                transform_vab = device->CreateVAB(graph::Assign::TransformID::VAB_FMT,
                                                 transform_vab_node_count,
                                                 nullptr,
                                                 graph::BufferAllocPolicy::Auto);
                transform_vab_buffer = transform_vab ? transform_vab->GetBuffer() : VK_NULL_HANDLE;
            }

            if (transform_vab)
            {
                graph::Assign::TransformID::ValueType* transform_ptr =
                    (graph::Assign::TransformID::ValueType*)(transform_vab->DeviceBuffer::Map());
                const uint32_t max_transform_id =
                    std::numeric_limits<graph::Assign::TransformID::ValueType>::max();
                bool warned_overflow = false;

                for (size_t i = 0; i < items.size(); ++i)
                {
                    RenderItem* item = items[i];
                    const uint32_t idx = item ? item->transform_index : 0;

                    if (idx > max_transform_id)
                    {
                        if (!warned_overflow && sizeof(graph::Assign::TransformID::ValueType) == sizeof(uint16_t))
                        {
                            std::cout << "[ECS::MaterialBatch::Finalize] WARNING: TransformID overflow ("
                                      << idx << ")" << std::endl;
                            warned_overflow = true;
                        }
                        *transform_ptr = static_cast<graph::Assign::TransformID::ValueType>(0);
                    }
                    else
                    {
                        *transform_ptr = static_cast<graph::Assign::TransformID::ValueType>(idx);
                    }

                    ++transform_ptr;
                }

                transform_vab->Unmap();
            }
        }
    }

    void MaterialBatch::ReallocICB(const std::vector<RenderItem*>& list,
                                   graph::IndirectDrawBuffer*& icb_draw_out,
                                   graph::IndirectDrawIndexedBuffer*& icb_draw_indexed_out)
    {
        if (!device || list.empty())
        {
            std::cout << "[ECS::MaterialBatch::ReallocICB] Cannot allocate - Device: "
                      << (void*)device << ", Items: " << list.size() << std::endl;
            return;
        }

        // Calculate required buffer size (power of 2)
        uint32_t icb_new_count = 1;
        while (icb_new_count < list.size())
            icb_new_count <<= 1;

        // If existing buffers are large enough, reuse them
        if (icb_draw_out && icb_new_count <= icb_draw_out->GetMaxCount())
        {
            //std::cout << "[ECS::MaterialBatch::ReallocICB] Reusing existing buffers (capacity: "
            //          << icb_draw->GetMaxCount() << ")" << std::endl;
            return;
        }

        // Delete old buffers
        if (icb_draw_out)
        {
            std::cout << "[ECS::MaterialBatch::ReallocICB] Deleting old indirect draw buffer" << std::endl;
            delete icb_draw_out;
        }

        if (icb_draw_indexed_out)
        {
            std::cout << "[ECS::MaterialBatch::ReallocICB] Deleting old indexed indirect draw buffer" << std::endl;
            delete icb_draw_indexed_out;
        }

        // Create new buffers
        icb_draw_out = device->CreateIndirectDrawBuffer(icb_new_count);
        icb_draw_indexed_out = device->CreateIndirectDrawIndexedBuffer(icb_new_count);
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

    void MaterialBatch::BuildBatches(const std::vector<RenderItem*>& list,
                                     graph::DrawBatchArray& batches,
                                     uint32_t& batch_count,
                                     graph::IndirectDrawBuffer*& icb_draw_out,
                                     graph::IndirectDrawIndexedBuffer*& icb_draw_indexed_out,
                                     const uint32_t base_instance)
    {
        const size_t count = list.size();
        if (count == 0)
        {
            std::cout << "[ECS::MaterialBatch::BuildBatches] No items to batch" << std::endl;
            batch_count = 0;
            batches.clear();
            return;
        }

        // Allocate indirect command buffers
        ReallocICB(list, icb_draw_out, icb_draw_indexed_out);

        if (!icb_draw_out || !icb_draw_indexed_out)
        {
            std::cout << "[ECS::MaterialBatch::BuildBatches] ERROR: Failed to allocate indirect buffers!" << std::endl;
            batch_count = 0;
            batches.clear();
            return;
        }

        // Map indirect command buffers
        VkDrawIndirectCommand* draw_cmd = icb_draw_out->MapCmd();
        VkDrawIndexedIndirectCommand* indexed_draw_cmd = icb_draw_indexed_out->MapCmd();

        // Prepare batch array
        batches.clear();
        batches.resize(count);

        // Initialize first batch
        graph::DrawBatch* batch = batches.data();
        RenderItem* item = list[0];
        graph::Primitive* primitive = item->GetPrimitive();

        if (!primitive)
        {
            std::cout << "[ECS::MaterialBatch::BuildBatches] ERROR: First item has no primitive!" << std::endl;
            icb_draw_out->Unmap();
            icb_draw_indexed_out->Unmap();
            batch_count = 0;
            batches.clear();
            return;
        }

        batch_count = 1;
        batch->first_instance = base_instance;
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
            item = list[i];
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
            ++batch_count;
            ++batch;

            batch->first_instance = base_instance + static_cast<uint32_t>(i);
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

        icb_draw_out->Unmap();
        icb_draw_indexed_out->Unmap();
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

    #ifdef _DEBUG
        if (transform_buffer && !items.empty())
        {
            uint32_t max_transform_index = 0;
            for (auto *item : items)
            {
                if (item && item->transform_index > max_transform_index)
                    max_transform_index = item->transform_index;
            }

            auto static_storage = TransformComponent::GetStaticStorage();
            auto dynamic_storage = TransformComponent::GetDynamicStorage();
            const uint32_t static_count = static_cast<uint32_t>(static_storage ? static_storage->GetSize() : 0);
            const uint32_t dynamic_count = static_cast<uint32_t>(dynamic_storage ? dynamic_storage->GetSize() : 0);
            const uint32_t total_count = transform_buffer->GetTotalCount(static_count, dynamic_count);

            if (total_count > 0 && max_transform_index >= total_count)
            {
                std::cout << "[ECS::MaterialBatch::Render] WARNING: Transform index out of range ("
                          << max_transform_index << " >= " << total_count << ")" << std::endl;
            }
        }
    #endif

        if (transform_buffer)
            transform_buffer->BindTransform(key.material);

        if (mi_buffer)
            mi_buffer->BindMaterialInstance(key.material);

        if (renderer && draw_batches_count > 0)
        {
            renderer->Render(cmdBuffer, draw_batches, draw_batches_count,
                           transform_buffer, mi_buffer, transform_vab_buffer,
                           icb_draw, icb_draw_indexed);
        }
        // else
        // {
        //     std::cout << "[ECS::MaterialBatch::Render] ERROR: No renderer available!" << std::endl;
        // }
    }

    void MaterialBatch::QueueMovableTransformUpdates()
    {
        if (!transform_buffer || static_count >= items.size())
            return;

        int first = std::numeric_limits<int>::max();
        int last = -1;

        for (size_t i = static_count; i < items.size(); ++i)
        {
            RenderItem* item = items[i];
            if (!item)
                continue;

            const int idx = static_cast<int>(item->transform_index);
            if (idx < first)
                first = idx;
            if (idx > last)
                last = idx;
        }

        if (last >= 0)
        {
            transform_buffer->UpdateTransformData(items, first, last);
        }
    }

}//namespace hgl::ecs
