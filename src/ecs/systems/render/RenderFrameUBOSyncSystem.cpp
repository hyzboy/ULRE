#include<hgl/ecs/systems/render/RenderFrameUBOSyncSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>

namespace hgl::ecs
{
    RenderFrameUBOSyncSystem::RenderFrameUBOSyncSystem(const std::string& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderFrameSync);
        AddDependency<RenderBufferUploadSystem>();
        AddDependency<EnvironmentSystem>();
        AddDependency<RenderTargetSystem>();
    }

    void RenderFrameUBOSyncSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto camera_system = context->GetSystem<CameraSystem>();
        if (camera_system)
            camera_system->SyncCameraUBO();
    }
}
