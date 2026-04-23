#include <hgl/ecs/support/sprite2d/Sprite2DRenderPipelineGroup.h>
#include <hgl/ecs/systems/render/Sprite2DResourcePrepareSystem.h>
#include <hgl/ecs/systems/render/Sprite2DMaterialBindingSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    Sprite2DRenderPipelineGroup::Sprite2DRenderPipelineGroup()
        : RenderPipelineGroup("Sprite2D")
    {
    }

    bool Sprite2DRenderPipelineGroup::Initialize(ECSContext* context)
    {
        if (!context)
            return false;

        // Register the two setup/binding systems
        context->RegisterRenderSystem<Sprite2DResourcePrepareSystem>();
        context->RegisterRenderSystem<Sprite2DMaterialBindingSystem>();

        // Provide world context to both systems (legacy SetWorld pattern)
        if (auto sys = context->GetSystem<Sprite2DResourcePrepareSystem>())
            sys->SetWorld(context);

        if (auto sys = context->GetSystem<Sprite2DMaterialBindingSystem>())
            sys->SetWorld(context);

        return true;
    }

    void Sprite2DRenderPipelineGroup::Shutdown(ECSContext* /*context*/)
    {
        systems_.clear();
        pipeline_.reset();
    }

    std::unique_ptr<RenderPipelineBase> Sprite2DRenderPipelineGroup::CreatePipeline()
    {
        // No dedicated pipeline — Sprite2D geometry goes through the Primitive pipeline
        return nullptr;
    }

    void Sprite2DRenderPipelineGroup::RegisterSystems()
    {
        // Systems registered directly to Context in Initialize()
    }

}  // namespace hgl::ecs
