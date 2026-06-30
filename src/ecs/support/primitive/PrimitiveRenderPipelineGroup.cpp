#include <hgl/ecs/support/primitive/PrimitiveRenderPipelineGroup.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/support/primitive/PrimitiveCullSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveSortSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveBuildSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveOverlayRenderSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    PrimitiveRenderPipelineGroup::PrimitiveRenderPipelineGroup()
        : RenderPipelineGroup("Primitive")
    {
    }

    bool PrimitiveRenderPipelineGroup::Initialize(ECSContext* context)
    {
        // 1. Create pipeline (needs context for PrimitiveBatchPipeline)
        auto pipeline = std::make_unique<PrimitiveRenderPipeline>(context);

        // 2. Register pipeline to Context
        context->RegisterRenderPipeline(name_, std::move(pipeline));

        // 3. Create and register systems (Context takes ownership via RegisterRenderSystem)
        context->RegisterRenderSystem<PrimitiveCullSystem>();
        context->RegisterRenderSystem<PrimitiveSortSystem>();
        context->RegisterRenderSystem<PrimitiveBuildSystem>();
        context->RegisterRenderSystem<PrimitiveRenderSystem>();
        context->RegisterRenderSystem<PrimitiveOverlayRenderSystem>();

        return true;
    }

    void PrimitiveRenderPipelineGroup::Shutdown(ECSContext* /*context*/)
    {
        systems_.clear();
        pipeline_.reset();
    }

}  // namespace hgl::ecs
