#include <hgl/ecs/support/primitive/PrimitiveRenderPipelineGroup.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/support/primitive/PrimitiveCullSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveSortSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveBuildSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
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

        return true;
    }

    void PrimitiveRenderPipelineGroup::Shutdown(ECSContext* /*context*/)
    {
        systems_.clear();
        pipeline_.reset();
    }

    std::unique_ptr<RenderPipelineBase> PrimitiveRenderPipelineGroup::CreatePipeline()
    {
        // Not used — pipeline is created in Initialize() with the context parameter
        return nullptr;
    }

    void PrimitiveRenderPipelineGroup::RegisterSystems()
    {
        // Not used — systems are registered to Context directly in Initialize()
    }

}  // namespace hgl::ecs
