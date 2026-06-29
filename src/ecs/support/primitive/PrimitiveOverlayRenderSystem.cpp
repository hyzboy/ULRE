#include <hgl/ecs/support/primitive/PrimitiveOverlayRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderSubmitStats.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/MaterialBatch.h>
#include <hgl/ecs/support/PipelineMaterialRenderer.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/ShaderMaterialProgramManager.h>

#include <algorithm>
#include <vector>

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

        std::vector<MaterialBatch *> ordered_batches;
        ordered_batches.reserve(cache.materialBatches.GetCount());

        for (auto &pair : cache.materialBatches)
        {
            MaterialBatch *batch = pair.second.get();
            if (!batch || batch->items.empty())
                continue;

            if (batch->key.queue != RenderQueue::Overlay)
                continue;

            ordered_batches.push_back(batch);
        }

        std::sort(ordered_batches.begin(), ordered_batches.end(),
                  [](const MaterialBatch *lhs, const MaterialBatch *rhs)
                  {
                      if (!lhs || !rhs)
                          return lhs != nullptr;

                      auto *lhs_program = lhs->key.GetProgram();
                      auto *rhs_program = rhs->key.GetProgram();
                      if (lhs_program != rhs_program)
                          return lhs_program < rhs_program;

                      if (lhs->key.domain != rhs->key.domain)
                          return lhs->key.domain < rhs->key.domain;

                      if (lhs->key.pipeline != rhs->key.pipeline)
                          return lhs->key.pipeline < rhs->key.pipeline;

                      return false;
                  });

        PrimitiveRenderSubmitStats submit_stats("[ECS::PrimitiveOverlayRenderSystem][R4]", false);

        for (MaterialBatch *batch : ordered_batches)
        {
            if (!batch || batch->items.empty())
                continue;

            const auto& key = batch->key;
            auto *key_program = key.GetProgram();
            if (!key_program || !key.pipeline)
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
                    domain_binding = material_manager->FindDomainMaterialBinding(key.domain, key_program);
            }

            const void *current_binding_token = domain_binding
                                              ? static_cast<const void *>(domain_binding)
                                              : static_cast<const void *>(key_program);

            bool skip_pipeline_bind = false;
            bool skip_descriptor_bind = false;
            submit_stats.OnBatch(key.pipeline,
                                 key_program,
                                 current_binding_token,
                                 skip_pipeline_bind,
                                 skip_descriptor_bind);

            renderer->Render(cmdBuffer,
                             batch->draw_batches,
                             batch->draw_batches_count,
                             batch->transform_buffer,
                             batch->mi_buffer,
                             batch->icb_draw,
                             batch->icb_draw_indexed,
                             domain_binding,
                             skip_pipeline_bind,
                             skip_descriptor_bind);
        }

        submit_stats.CommitAndMaybeLog(context);
    }
}
