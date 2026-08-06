#include <hgl/ecs/support/primitive/PrimitiveOverlayRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/MaterialBatch.h>
#include <hgl/ecs/support/PipelineMaterialRenderer.h>
#include <hgl/vk/VKCommandBuffer.h>

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
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderDebug);
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

        for (auto& pair : cache.materialBatches)
        {
            MaterialBatch* batch = pair.second.get();
            if (!batch || batch->items.empty())
                continue;

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
                             batch->icb_draw_indexed,
                             batch,
                             context->GetRenderContext());
        }
    }
}
