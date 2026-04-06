#include <hgl/ecs/support/line/LineRenderPipelineGroup.h>
#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/ecs/support/line/LineCollectSystem.h>
#include <hgl/ecs/support/line/LineBuildSystem.h>
#include <hgl/ecs/support/line/LineRenderSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    LineRenderPipelineGroup::LineRenderPipelineGroup()
        : RenderPipelineGroup("Line")
    {
    }

    bool LineRenderPipelineGroup::Initialize(ECSContext* context)
    {
        if (!context)
            return false;

        // 1. Create the pipeline and register it with the context
        auto pipeline = std::make_unique<LineRenderPipeline>(context);
        context->RegisterRenderPipeline(name_, std::move(pipeline));

        // 2. Register pipeline-driven systems
        context->RegisterRenderSystem<LineCollectSystem>();
        context->RegisterRenderSystem<LineBuildSystem>();
        context->RegisterRenderSystem<LineRenderSystem>();

        return true;
    }

    void LineRenderPipelineGroup::Shutdown(ECSContext* /*context*/)
    {
        systems_.clear();
        pipeline_.reset();
    }

    std::unique_ptr<RenderPipelineBase> LineRenderPipelineGroup::CreatePipeline()
    {
        // GraphicsPipeline created in Initialize() with context
        return nullptr;
    }

    void LineRenderPipelineGroup::RegisterSystems()
    {
        // Systems registered to Context in Initialize()
    }

}  // namespace hgl::ecs
