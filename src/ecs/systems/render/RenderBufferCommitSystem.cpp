#include<hgl/ecs/systems/render/RenderBufferCommitSystem.h>

namespace hgl::ecs
{
    // RenderBufferCommitSystem is deprecated.
    // Commit logic has been merged into RenderBufferUploadSystem.
    // This class is retained as an empty stub for link compatibility.

    RenderBufferCommitSystem::RenderBufferCommitSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderBufferCommit);
    }

    void RenderBufferCommitSystem::Update(float /*deltaTime*/)
    {
        // No-op: commit queue removed, upload handled by RenderBufferUploadSystem
    }
}//namespace hgl::ecs

