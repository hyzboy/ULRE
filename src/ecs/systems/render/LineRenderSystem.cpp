#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/graph/geo/line/LineRenderService.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::ecs
{
    LineRenderSystem::LineRenderSystem(const std::string &name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderPostProcess);
        AddDependency<RenderPrimitiveSubmitSystem>();
    }

    LineRenderSystem::~LineRenderSystem()
    {
    }

    void LineRenderSystem::SetLineRenderService(graph::LineRenderService *svc)
    {
        line_service = svc;
        if (!line_service)
            return;

        if (render_context)
            line_service->SetRenderContext(render_context);

        if (render_target)
            line_service->SetRenderTarget(render_target);
    }

    void LineRenderSystem::SetRenderContext(graph::RenderContext *ctx)
    {
        render_context = ctx;
        if (line_service)
            line_service->SetRenderContext(ctx);
    }

    void LineRenderSystem::SetRenderTarget(graph::IRenderTarget *rt)
    {
        if (render_target == rt)
            return;

        render_target = rt;
        if (line_service)
            line_service->SetRenderTarget(rt);
    }

    void LineRenderSystem::Render(graph::RenderCmdBuffer *cmd, float /*deltaTime*/)
    {
        if (!cmd || !line_service)
            return;

        line_service->Draw(cmd);
    }
}//namespace hgl::ecs

