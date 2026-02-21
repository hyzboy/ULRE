#include<hgl/ecs/systems/render/RenderBufferCommitSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VKBufferCommitQueue.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::ecs
{
    RenderBufferCommitSystem::RenderBufferCommitSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderBufferCommit);
    }

    void RenderBufferCommitSystem::Update(float /*deltaTime*/)
    {
        if (!world || !device)
            return;

        auto *commit_queue = device->GetBufferCommitQueue();
        if (!commit_queue)
            return;

        const uint32_t frame_index = world->GetFrameIndex();
        commit_queue->BeginFrame(frame_index);

        if (commit_queue->HasPending())
            commit_queue->CommitAll();
    }
}//namespace hgl::ecs

