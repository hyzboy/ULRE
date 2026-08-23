#include<hgl/ecs/systems/render/ViewUBOCommitSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/render/RenderContext.h>

namespace hgl::ecs
{
    ViewUBOCommitSystem::ViewUBOCommitSystem(const std::string &name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderBufferCommit);
    }

    void ViewUBOCommitSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        if (auto camera_system = context->GetSystem<CameraSystem>())
            camera_system->CommitCameraUBO();

        if (auto rdb = context->GetSystem<RenderDescriptorBindingSystem>())
            rdb->CommitViewportUBO();

        graph::GraphicsContext *gc = nullptr;
        if (auto *rc = context->GetRenderContext())
            gc = rc->GetGraphicsContext();
        if (!gc)
            gc = context->GetGraphicsContext();

        if (auto *env_manager = gc ? gc->GetEnvironmentManager() : nullptr)
            env_manager->CommitMaterialized();
    }
}//namespace hgl::ecs
