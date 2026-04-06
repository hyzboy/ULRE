#include<hgl/ecs/systems/render/RenderFrameBusinessSyncSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>

namespace hgl::ecs
{
    RenderFrameBusinessSyncSystem::RenderFrameBusinessSyncSystem(const std::string& name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderFrameSync);
        AddDependency<RenderBufferUploadSystem>();
        AddDependency<EnvironmentSystem>();
        AddDependency<RenderTargetSystem>();
    }

    void RenderFrameBusinessSyncSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto camera_system = context->GetSystem<CameraSystem>();
        if (camera_system)
            camera_system->SyncCameraUBO();

        auto environment_system = context->GetSystem<EnvironmentSystem>();
        if (environment_system)
            environment_system->SyncSkyUBO();
    }
}
