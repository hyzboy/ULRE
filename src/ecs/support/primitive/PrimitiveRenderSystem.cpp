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

        graph::GraphicsPipeline* last_pipeline = nullptr;

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
                if (!batch->items.empty() && batch->items.front())
                {
                    const auto state = batch->items.front()->GetResolvedMaterialState();
                    if (state.binding_instance)
                    {
                        domain_binding = hgl::graph::MaterialBindingInstanceInternalAccess::GetDomainBinding(state.binding_instance);
                        if (domain_binding)
                            binding_source = "item-mi";

                        LogInfo("[TBV][PrimitiveRenderSystem] Batch first-item state: batch=%p item=%p material=%p(%s) domain=%p mi=%p mi.binding=%p mi_id=%d preset=%u",
                                static_cast<void *>(batch),
                                static_cast<void *>(batch->items.front()),
                                static_cast<void *>(state.material),
                                state.material ? state.material->GetName().c_str() : "<null>",
                                static_cast<void *>(state.domain),
                                static_cast<void *>(state.binding_instance),
                                static_cast<void *>(domain_binding),
                                state.mi_id,
                                static_cast<unsigned>(state.preset));
                    }
                }

                if (!domain_binding)
                {
                    auto *graphics_context = context->GetGraphicsContext();
                    auto *material_manager = graphics_context ? graphics_context->GetMaterialManager() : nullptr;
                    if (material_manager)
                    {
                        domain_binding = material_manager->FindDomainMaterialBinding(key.domain, key.material);
                        if (domain_binding)
                            binding_source = "manager";
                    }
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
