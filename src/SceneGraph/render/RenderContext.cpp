#include<hgl/graph/RenderContext.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>

namespace hgl::graph
{
    RenderContext::RenderContext(RenderFramework *rf,IRenderTarget *rt)
    {
        this->rf = rf;
        render_target = rt;
        ecs_context = nullptr;
    }

    RenderContext::~RenderContext()
    {
        ecs_context = nullptr;
        render_target = nullptr;
    }

    static void SyncRenderTargetSystem(hgl::ecs::ECSContext* ctx,
                                       RenderFramework* rf,
                                       IRenderTarget* rt)
    {
        if(!ctx)
            return;

        auto render_target_system = ctx->GetSystem<hgl::ecs::RenderTargetSystem>();
        if(!render_target_system)
            return;

        render_target_system->SetRenderFramework(rf);
        render_target_system->SetRenderTarget(rt);
    }

    void RenderContext::SetRenderTarget(IRenderTarget *rt)
    {
        render_target = rt;
        SyncRenderTargetSystem(ecs_context, rf, render_target);
    }

    void RenderContext::SetECSContext(ecs::ECSContext *ctx)
    {
        ecs_context = ctx;
        SyncRenderTargetSystem(ecs_context, rf, render_target);
    }

    void RenderContext::Tick(double)
    {
    }

}//namespace hgl::graph

