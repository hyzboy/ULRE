#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/MaterialBatch.h>
#include <hgl/ecs/support/PipelineMaterialRenderer.h>
#include <hgl/ecs/support/TransformAssignmentBuffer.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/support/TransformDataStorage.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/log/Log.h>
#include <algorithm>
#include <vector>

namespace hgl::ecs
{
    namespace
    {
        int QueueSortRank(const RenderQueue queue)
        {
            switch (queue)
            {
            case RenderQueue::Opaque:      return 0;
            case RenderQueue::Masked:      return 1;
            case RenderQueue::Transparent: return 2;
            case RenderQueue::Overlay:     return 3;
            default:                       return 99;
            }
        }
    }

    PrimitiveRenderSystem::PrimitiveRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderDrawSubmit);
        SetRenderElementType("Primitive");
    }

    RenderPipelineBase* PrimitiveRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Primitive");
    }

    void PrimitiveRenderSystem::OnRender(RenderPipelineBase* /*pipeline*/, hgl::graph::RenderCmdBuffer* cmdBuffer)
    {
        if (!context || !cmdBuffer)
            return;

        auto& cache = context->GetRenderFrameCache();
        if (cache.renderableCount == 0)
            return;

        std::vector<MaterialBatch*> ordered_batches;
        ordered_batches.reserve(cache.materialBatches.GetCount());

        for (auto& pair : cache.materialBatches)
        {
            MaterialBatch* batch = pair.second.get();
            if (!batch || batch->items.empty())
                continue;

            ordered_batches.push_back(batch);
        }

        std::sort(ordered_batches.begin(), ordered_batches.end(),
                  [](const MaterialBatch* lhs, const MaterialBatch* rhs)
                  {
                      if (!lhs || !rhs)
                          return lhs != nullptr;

                      const int lhs_rank = QueueSortRank(lhs->key.queue);
                      const int rhs_rank = QueueSortRank(rhs->key.queue);
                      if (lhs_rank != rhs_rank)
                          return lhs_rank < rhs_rank;

                      if (lhs->key.material != rhs->key.material)
                          return lhs->key.material < rhs->key.material;

                      if (lhs->key.pipeline != rhs->key.pipeline)
                          return lhs->key.pipeline < rhs->key.pipeline;

                      return lhs->key.domain < rhs->key.domain;
                  });

        for (MaterialBatch* batch : ordered_batches)
        {
            if (!batch || batch->items.empty())
                continue;

            const bool is_line_primitive = batch->key.material
                && batch->key.material->GetPrimitiveType() == graph::PrimitiveType::Lines;

            if (batch->key.queue == RenderQueue::Overlay)
            {
                if (is_line_primitive)
                {
                    LogDebug("[WireTrace] Render skip line batch: queue=Overlay material=%s items=%zu",
                             batch->key.material ? batch->key.material->GetName().c_str() : "<null>",
                             batch->items.size());
                }
                continue;
            }

            const auto& key = batch->key;
            if (!key.material || !key.pipeline)
            {
                if (is_line_primitive)
                {
                    LogDebug("[WireTrace] Render skip line batch: material/pipeline missing material=%p pipeline=%p",
                             static_cast<void *>(key.material),
                             static_cast<void *>(key.pipeline));
                }
                continue;
            }

            if (batch->draw_batches_count == 0)
            {
                if (is_line_primitive)
                {
                    LogDebug("[WireTrace] Render skip line batch: draw_batches_count=0 material=%s items=%zu",
                             key.material->GetName().c_str(),
                             batch->items.size());
                }
                continue;
            }

            auto* renderer = batch->renderer;
            if (!renderer)
            {
                if (is_line_primitive)
                {
                    LogDebug("[WireTrace] Render skip line batch: renderer is null material=%s",
                             key.material->GetName().c_str());
                }
                continue;
            }

            if (is_line_primitive)
            {
                LogDebug("[WireTrace] Render submit line batch: material=%s queue=%u items=%zu draw_batches=%u pipeline=%p",
                         key.material->GetName().c_str(),
                         static_cast<unsigned>(key.queue),
                         batch->items.size(),
                         batch->draw_batches_count,
                         static_cast<void *>(key.pipeline));
            }

            renderer->Render(cmdBuffer,
                             batch->draw_batches,
                             batch->draw_batches_count,
                             batch->transform_buffer,
                             batch->mi_buffer,
                             batch->icb_draw,
                             batch->icb_draw_indexed);
        }
    }
}
