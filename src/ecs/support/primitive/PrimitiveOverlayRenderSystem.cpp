#include <hgl/ecs/support/primitive/PrimitiveOverlayRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
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

                      if (lhs->key.material != rhs->key.material)
                          return lhs->key.material < rhs->key.material;

                      if (lhs->key.domain != rhs->key.domain)
                          return lhs->key.domain < rhs->key.domain;

                      if (lhs->key.pipeline != rhs->key.pipeline)
                          return lhs->key.pipeline < rhs->key.pipeline;

                      return false;
                  });

        graph::GraphicsPipeline* last_pipeline = nullptr;
        const void *last_binding_token = nullptr;

        uint64_t frame_batches_seen = 0;
        uint64_t frame_program_bind_calls = 0;
        uint64_t frame_descriptor_bind_calls = 0;
        uint64_t frame_program_switches = 0;
        uint64_t frame_descriptor_switches = 0;

        graph::ShaderMaterialProgram *last_program = nullptr;

        for (MaterialBatch *batch : ordered_batches)
        {
            if (!batch || batch->items.empty())
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

            ++frame_batches_seen;

            graph::ShaderMaterialProgram *current_program = key.material;
            if (current_program && current_program != last_program)
            {
                ++frame_program_switches;
                last_program = current_program;
            }

            const void *current_binding_token = domain_binding
                                              ? static_cast<const void *>(domain_binding)
                                              : static_cast<const void *>(key.material);
            const bool binding_changed = (current_binding_token != last_binding_token);
            if (current_binding_token && binding_changed)
                ++frame_descriptor_switches;

            const bool skip_pipeline_bind = (key.pipeline == last_pipeline);
            last_pipeline = key.pipeline;
            if (!skip_pipeline_bind)
                ++frame_program_bind_calls;

            const bool skip_descriptor_bind = !binding_changed
                                           && (current_binding_token != nullptr)
                                           && (frame_batches_seen > 1);
            if (!skip_descriptor_bind)
                ++frame_descriptor_bind_calls;

            last_binding_token = current_binding_token;

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

        auto &diag = context->GetMaterialResolveDiagnostics();
        diag.RecordR4RenderSubmitStats(frame_batches_seen,
                                       frame_program_bind_calls,
                                       frame_descriptor_bind_calls,
                                       frame_program_switches,
                                       frame_descriptor_switches);
    }
}
