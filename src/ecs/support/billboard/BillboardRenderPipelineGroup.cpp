#include <hgl/ecs/support/billboard/BillboardRenderPipelineGroup.h>
#include <hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include <hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    BillboardRenderPipelineGroup::BillboardRenderPipelineGroup()
        : RenderPipelineGroup("Billboard")
    {
    }

    bool BillboardRenderPipelineGroup::Initialize(ECSContext* context)
    {
        if (!context)
            return false;

        // Register the two setup/binding systems
        context->RegisterRenderSystem<QuadResourcePrepareSystem>();
        context->RegisterRenderSystem<QuadMaterialBindingSystem>();

        // Provide world context to both systems (legacy SetWorld pattern)
        if (auto sys = context->GetSystem<QuadResourcePrepareSystem>())
            sys->SetWorld(context);

        if (auto sys = context->GetSystem<QuadMaterialBindingSystem>())
            sys->SetWorld(context);

        return true;
    }

    void BillboardRenderPipelineGroup::Shutdown(ECSContext* /*context*/)
    {
        systems_.clear();
        pipeline_.reset();
    }

}  // namespace hgl::ecs
