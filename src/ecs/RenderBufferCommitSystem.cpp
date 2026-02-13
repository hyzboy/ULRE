#include<hgl/ecs/RenderBufferCommitSystem.h>
#include<hgl/ecs/Context.h>
#include<hgl/graph/VKBufferCommitQueue.h>
#include<hgl/graph/VKDevice.h>

namespace hgl::ecs
{
    RenderBufferCommitSystem::RenderBufferCommitSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderBeginFrame);
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
