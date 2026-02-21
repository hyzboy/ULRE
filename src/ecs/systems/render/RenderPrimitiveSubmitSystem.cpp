#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchFinalizeSystem.h>
#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/log/Log.h>
#include<hgl/ecs/support/PipelineMaterialRenderer.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>

namespace hgl::ecs
{
    RenderPrimitiveSubmitSystem::RenderPrimitiveSubmitSystem(const std::string& name)
        : System(name)
    {
        // Set system type and properties
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderDrawSubmit);

        // Declare dependencies
        AddDependency<RenderPrimitiveBatchFinalizeSystem>(); // Needs batched data
        AddDependency<RenderBufferUploadSystem>();           // Needs uploaded buffers ready
    }

    void RenderPrimitiveSubmitSystem::Render(graph::RenderCmdBuffer* cmdBuffer, float /*deltaTime*/)
    {
        if (!world || !cmdBuffer)
            return;

        auto& cache = world->GetRenderFrameCache();

        if (cache.renderableCount == 0)
        {
            if (!cache.materialBatches.IsEmpty())
            {
                LogWarning("[RenderPrimitiveSubmitSystem] No renderables but material batches exist (%zu)",
                           cache.materialBatches.GetCount());
            }
            return;
        }

        for (auto& pair : cache.materialBatches)
        {
            MaterialBatch* batch = pair.second.get();
            if (!batch || batch->items.empty())
                continue;

            const auto& key = batch->key;
            if (!key.material || !key.pipeline)
            {
                LogError("[RenderPrimitiveSubmitSystem::Render] Missing material or pipeline");
                continue;
            }

            if (batch->draw_batches_count == 0)
            {
                LogError("[RenderPrimitiveSubmitSystem::Render] No batches to render");
                continue;
            }

        #ifdef _DEBUG
            if (auto *transform_buffer = batch->transform_buffer)
            {
                const auto& items = batch->items;
                uint32_t max_transform_index = 0;
                for (auto *item : items)
                {
                    if (item && item->transform_index > max_transform_index)
                        max_transform_index = item->transform_index;
                }

                auto storage = TransformComponent::GetSharedStorage();
                uint32_t static_count = 0;
                uint32_t dynamic_count = 0;

                if (storage)
                {
                    const size_t total = storage->GetSize();
                    for (size_t i = 0; i < total; ++i)
                    {
                        if (storage->GetMobility(static_cast<TransformDataStorage::HandleID>(i)) == 0)
                            ++static_count;
                        else
                            ++dynamic_count;
                    }
                }

                const uint32_t total_count = transform_buffer->GetTotalCount(static_count, dynamic_count);
                if (total_count > 0 && max_transform_index >= total_count)
                {
                    LogWarning("[RenderPrimitiveSubmitSystem::Render] Transform index out of range (%u >= %u)",
                               max_transform_index,
                               total_count);
                }
            }
        #endif

            auto *renderer = batch->renderer;
            if (!renderer)
                continue;

            renderer->Render(cmdBuffer,
                             batch->draw_batches,
                             batch->draw_batches_count,
                             batch->transform_buffer,
                             batch->mi_buffer,
                             batch->transform_vab_buffer,
                             batch->icb_draw,
                             batch->icb_draw_indexed);
        }
    }
}//namespace hgl::ecs

