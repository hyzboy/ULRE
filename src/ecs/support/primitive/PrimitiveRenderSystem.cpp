#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/MaterialBatch.h>
#include <hgl/ecs/support/PipelineMaterialRenderer.h>
#include <hgl/ecs/support/TransformAssignmentBuffer.h>
#include <hgl/ecs/support/TransformAccessor.h>
#include <hgl/ecs/support/TransformDataStorage.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/log/Log.h>
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

            // EDS：覆盖层标记属于**渲染形态**（材质/批次事实），不在 Pipeline 对象上
            //（两个 recipe 只差 overlay 时会共用同一 Pipeline ⇒ 读 Pipeline 会串味）。
            return batch->key.pipeline_config.overlay;
        }
    }

    PrimitiveRenderSystem::PrimitiveRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderDrawSubmit);
        SetRenderElementType("Primitive");
    }

    RenderPipelineBase* PrimitiveRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Primitive");
    }

    void PrimitiveRenderSystem::RenderBatches(ECSContext* context, hgl::graph::RenderCmdBuffer* cmdBuffer, bool overlay_only)
    {
        if (!context || !cmdBuffer)
            return;

        auto& cache = context->GetRenderFrameCache();
        if (cache.renderableCount == 0)
            return;

        auto& ordered_batches = cache.ordered_batches;
        ordered_batches.clear();
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

        for (MaterialBatch* batch : ordered_batches)
        {
            if (!batch || batch->items.empty())
                continue;

            const bool is_overlay = IsOverlayLikeBatch(batch);
            if (overlay_only != is_overlay)
                continue;

            const auto& key = batch->key;
            if (!key.shader_program || !key.pipeline)
                continue;

            auto* renderer = batch->renderer;
            if (!renderer)
                continue;

            renderer->Render(cmdBuffer,
                             batch->transform_buffer,
                             batch,
                             context->GetRenderContext(),
                             context->GetActiveCameraRow(),
                             context->IsCurrentPassShadow() ? RenderPassSlot::Shadow : RenderPassSlot::Main);
        }
    }

    void PrimitiveRenderSystem::OnRender(RenderPipelineBase* /*pipeline*/, hgl::graph::RenderCmdBuffer* cmdBuffer)
    {
        RenderBatches(context, cmdBuffer, false);
    }
}
