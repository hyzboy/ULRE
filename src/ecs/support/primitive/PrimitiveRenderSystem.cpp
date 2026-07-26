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
        bool IsOverlayLikeBatch(const MaterialBatch* batch)
        {
            if (!batch || !batch->key.pipeline)
                return false;

            const auto *pd = batch->key.pipeline->GetData();
            if (!pd || !pd->depth_stencil)
                return false;

            // Overlay-style depth state: pass regardless of depth and do not write depth.
            return pd->depth_stencil->depthCompareOp == VK_COMPARE_OP_ALWAYS
                && pd->depth_stencil->depthWriteEnable == VK_FALSE;
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

        for (MaterialBatch* batch : ordered_batches)
        {
            if (!batch || batch->items.empty())
                continue;

            if (!batch->descriptor_bind_valid)
                continue;

            if (IsOverlayLikeBatch(batch))
                continue;

            const auto& key = batch->key;
            if (!key.material || !key.pipeline)
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
