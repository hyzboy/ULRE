#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::ecs
{
    RenderTargetSystem::RenderTargetSystem(const std::string &name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);
    }

    void RenderTargetSystem::SetRenderContext(graph::RenderContext *ctx)
    {
        render_context = ctx;
    }

    void RenderTargetSystem::SetRenderTarget(graph::IRenderTarget *rt)
    {
        render_target = rt;
        SyncSubsystems();
    }

    void RenderTargetSystem::Update(float /*deltaTime*/)
    {
        if (!render_context && context)
            render_context = context->GetRenderContext();

        if (!render_target && context)
            render_target = context->GetRenderTarget();

        if (!render_target)
            return;

        SyncSubsystems();
    }

    void RenderTargetSystem::SyncSubsystems()
    {
        if (!context)
            return;

        // 原 render_context->SetCurrentRenderTarget(render_target) 已随 RenderContext
        // 双状态删除而移除：当前渲染目标唯一权威在 ECSContext::render_target
        //（本系统 SetRenderTarget 时已同步 context 侧指针），消费方一律读它。

        auto camera_system = context->GetSystem<CameraSystem>();
        if (camera_system)
        {
            camera_system->SetRenderContext(render_context);
            camera_system->SetViewportInfo(render_target ? render_target->GetViewportInfo() : nullptr);
        }

        // CN: LineRenderSystem 会在 Render 时延迟初始化，自动获取 RenderTarget 信息
        // EN: LineRenderSystem will lazy-init on first Render, automatically get RenderTarget info
    }
}//namespace hgl::ecs

