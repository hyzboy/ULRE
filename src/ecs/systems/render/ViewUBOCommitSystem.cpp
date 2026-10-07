#include<hgl/ecs/systems/render/ViewUBOCommitSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/vk/VKRenderTarget.h>

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

        if (auto rdb = context->GetSystem<RenderSceneUBOSystem>())
            rdb->CommitViewportUBO();

        // 本世界的影子数据（T6b/P4：世界私有，不再经 env profile）：只在**交换链帧 acquire
        // 之后**写它那一槽 —— 该槽（交换链图像）的上一帧已在 NextFrame 里等完，写它不会踩
        // 仍在飞的其它帧（它们读自己的槽）。离屏 pass 不采样影子数据、且发生在 acquire 之前
        // ⇒ 那里不提交。天空（sky）是单份 buffer：数据变时 `MarkDirty` 即时写，不需要每帧重提。
        if (auto *rt = context->GetRenderTarget())
        {
            if (rt->IsSwapchain())
                context->CommitShadowInfo(rt->GetCurrentFrameIndex());
        }
    }
}//namespace hgl::ecs
