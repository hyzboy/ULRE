#include<hgl/ecs/systems/render/ViewUBOCommitSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/render/RenderContext.h>
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

        if (auto camera_system = context->GetSystem<CameraSystem>())
            camera_system->CommitCameraUBO();

        if (auto rdb = context->GetSystem<RenderSceneUBOSystem>())
            rdb->CommitViewportUBO();

        graph::GraphicsContext *gc = nullptr;
        if (auto *rc = context->GetRenderContext())
            gc = rc->GetGraphicsContext();
        if (!gc)
            gc = context->GetGraphicsContext();

        if (auto *env_manager = gc ? gc->GetEnvironmentManager() : nullptr)
        {
            // 只有交换链帧在 acquire 之后写 shadow 槽：该 acquired image 的上一帧
            // 已经在 NextFrame 里等完，其它在途帧读的是自己的槽。离屏 shadow pass
            // 不采样 ShadowInfo，且发生在 acquire 之前，写任何槽都可能踩主帧。
            uint32_t shadow_frame = 0;
            bool commit_shadow = false;
            if (auto *rt = context->GetRenderTarget())
            {
                if (rt->IsSwapchain())
                {
                    shadow_frame = rt->GetCurrentFrameIndex();
                    commit_shadow = true;
                }
            }
            env_manager->CommitMaterialized(shadow_frame, commit_shadow);
        }
    }
}//namespace hgl::ecs
