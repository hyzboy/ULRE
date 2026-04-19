#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/MaterialBatch.h>
#include <hgl/ecs/support/PipelineMaterialRenderer.h>
#include <hgl/ecs/support/TransformAssignmentBuffer.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/support/TransformDataStorage.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/ShaderMaterialProgramManager.h>
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

            if (batch->key.queue == RenderQueue::Overlay)
                continue;

            const auto& key = batch->key;
            if (!key.material || !key.pipeline)
                continue;

            if (batch->draw_batches_count == 0)
                continue;

            auto* renderer = batch->renderer;
            if (!renderer)
                continue;

            graph::DomainResourceBinding *domain_binding = nullptr;
            if (key.domain)
            {
                auto *graphics_context = context->GetGraphicsContext();
                auto *material_manager = graphics_context ? graphics_context->GetMaterialManager() : nullptr;
                if (material_manager)
                    domain_binding = material_manager->FindDomainMaterialBinding(key.domain, key.material);
            }

            renderer->Render(cmdBuffer,
                             batch->draw_batches,
                             batch->draw_batches_count,
                             batch->transform_buffer,
                             batch->mi_buffer,
                             batch->icb_draw,
                             batch->icb_draw_indexed,
                             domain_binding);
        }
    }
}
