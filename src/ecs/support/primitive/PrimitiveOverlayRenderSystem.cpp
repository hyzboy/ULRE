#include <hgl/ecs/support/primitive/PrimitiveOverlayRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/MaterialBatch.h>
#include <hgl/ecs/support/PipelineMaterialRenderer.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/ShaderMaterialProgramManager.h>

namespace hgl::ecs
{
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

        graph::GraphicsPipeline* last_pipeline = nullptr;

        for (auto& pair : cache.materialBatches)
        {
            MaterialBatch* batch = pair.second.get();
            if (!batch || batch->items.empty())
                continue;

            if (batch->key.queue != RenderQueue::Overlay)
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

            const bool skip_pipeline_bind = (key.pipeline == last_pipeline);
            last_pipeline = key.pipeline;

            renderer->Render(cmdBuffer,
                             batch->draw_batches,
                             batch->draw_batches_count,
                             batch->transform_buffer,
                             batch->mi_buffer,
                             batch->icb_draw,
                             batch->icb_draw_indexed,
                             domain_binding,
                             skip_pipeline_bind);
        }
    }
}
