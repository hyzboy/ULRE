#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/MaterialBatch.h>
#include <hgl/ecs/support/PipelineMaterialRenderer.h>
#include <hgl/ecs/core/RenderItem.h>
#include <hgl/ecs/support/TransformAssignmentBuffer.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/ecs/support/TransformDataStorage.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
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
        {
            LogInfo("[ECS::PrimitiveRenderSystem] skip draw submit: renderableCount=0 materialBatches=%u",
                    static_cast<unsigned>(cache.materialBatches.GetCount()));
            return;

        }

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
            const char *binding_source = "none";
            if (key.domain)
            {
                // Prefer the binding cached by RenderDescriptorBindingSystem (covers all primitives,
                // not just those with a runtime_texture_binding).
                if (batch->domain_binding)
                {
                    domain_binding = batch->domain_binding;
                    binding_source = "batch-domain";
                }
            }

            if (domain_binding)
            {
                auto *pm = domain_binding->GetPerMaterialMP();
                auto *po = domain_binding->GetPerObjectMP();
                LogInfo("[TBV][PrimitiveRenderSystem] Batch render binding selected: batch=%p material=%p(%s) domain=%p source=%s binding=%p pm=%p pm_ds=%p po=%p po_ds=%p item_count=%zu draw_batches=%u",
                        static_cast<void *>(batch),
                        static_cast<void *>(key.material),
                        key.material ? key.material->GetName().c_str() : "<null>",
                        static_cast<void *>(key.domain),
                        binding_source,
                        static_cast<void *>(domain_binding),
                        static_cast<void *>(pm),
                        pm ? (void *)pm->GetVkDescriptorSet() : nullptr,
                        static_cast<void *>(po),
                        po ? (void *)po->GetVkDescriptorSet() : nullptr,
                        batch->items.size(),
                        batch->draw_batches_count);
            }
            else if (key.domain)
            {
                LogInfo("[TBV][PrimitiveRenderSystem] Batch render binding missing: batch=%p material=%p(%s) domain=%p source=%s item_count=%zu draw_batches=%u",
                        static_cast<void *>(batch),
                        static_cast<void *>(key.material),
                        key.material ? key.material->GetName().c_str() : "<null>",
                        static_cast<void *>(key.domain),
                        binding_source,
                        batch->items.size(),
                        batch->draw_batches_count);
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
            {
                ++frame_descriptor_switches;
            }

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

        static uint64_t s_r4_submit_frames = 0;
        ++s_r4_submit_frames;
        if ((s_r4_submit_frames & (s_r4_submit_frames - 1)) == 0)
        {
            GLogInfo("[ECS::PrimitiveRenderSystem][R4] submit frame=%llu batches=%llu program_bind=%llu descriptor_bind=%llu program_switch=%llu descriptor_switch=%llu",
                     static_cast<unsigned long long>(s_r4_submit_frames),
                     static_cast<unsigned long long>(frame_batches_seen),
                     static_cast<unsigned long long>(frame_program_bind_calls),
                     static_cast<unsigned long long>(frame_descriptor_bind_calls),
                     static_cast<unsigned long long>(frame_program_switches),
                     static_cast<unsigned long long>(frame_descriptor_switches));
        }
    }
}
