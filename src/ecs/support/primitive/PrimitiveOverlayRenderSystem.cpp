#include <hgl/ecs/support/primitive/PrimitiveOverlayRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/MaterialBatch.h>
#include <hgl/ecs/support/PipelineMaterialRenderer.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <algorithm>
#include <vector>

namespace hgl::ecs
{
    namespace
    {
        bool IsOverlayLikeBatch(const MaterialBatch* batch)
        {
            if (!batch || !batch->key.pipeline)
                return false;

            return batch->key.pipeline->GetOverlay();
        }
    }

    PrimitiveOverlayRenderSystem::PrimitiveOverlayRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderDebug);
        SetRenderElementType("Primitive");
    }

    RenderPipelineBase* PrimitiveOverlayRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Primitive");
    }

    void PrimitiveOverlayRenderSystem::OnRender(RenderPipelineBase* /*pipeline*/, hgl::graph::RenderCmdBuffer* cmdBuffer)
    {
        if (!context || !cmdBuffer)
            return;

        auto& cache = context->GetRenderFrameCache();
        if (cache.renderableCount == 0)
            return;

        std::vector<MaterialBatch *> ordered_batches;
        ordered_batches.reserve(cache.materialBatches.GetCount());
        for (auto& pair : cache.materialBatches)
        {
            MaterialBatch* batch = pair.second.get();
            if (!batch || batch->items.empty())
                continue;

            ordered_batches.push_back(batch);
        }

        std::sort(ordered_batches.begin(),
                  ordered_batches.end(),
                  [](const MaterialBatch *a, const MaterialBatch *b)
                  {
                      return a->key < b->key;
                  });

        for (MaterialBatch *batch : ordered_batches)
        {
            if (!batch->descriptor_bind_valid)
                continue;

            if (!IsOverlayLikeBatch(batch))
                continue;

            const auto& key = batch->key;
            if (!key.shader_program || !key.pipeline)
                continue;

            if (batch->draw_batches_count == 0)
                continue;

            auto* renderer = batch->renderer;
            if (!renderer)
                continue;

            renderer->Render(cmdBuffer,
                             batch->draw_batches,
                             batch->draw_batches_count,
                             batch->transform_buffer,
                             batch->icb_draw,
                             batch,
                             context->GetRenderContext());
        }
    }
}
