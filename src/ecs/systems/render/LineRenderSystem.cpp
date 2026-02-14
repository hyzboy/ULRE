#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/graph/render/RenderFramework.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::graph
{
    LineRenderManager *CreateLineRenderManager(RenderFramework *, IRenderTarget *);
    LineRenderManager *CreateLineRenderManager(IGraphicsContext *, IRenderTarget *);
}

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
        delete line_manager;
    }

    void LineRenderSystem::SetRenderFramework(graph::RenderFramework *rf)
    {
        if (render_framework == rf)
            return;

        render_framework = rf;
        EnsureLineManager();
    }

    void LineRenderSystem::SetRenderTarget(graph::IRenderTarget *rt)
    {
        if (render_target == rt)
            return;

        render_target = rt;

        if (line_manager)
            line_manager->SetRenderTarget(rt);
        else
            EnsureLineManager();
    }

    void LineRenderSystem::EnsureLineManager()
    {
        if (line_manager || !render_target)
            return;

        if (context && context->GetGraphicsContext())
        {
            line_manager = CreateLineRenderManager(context->GetGraphicsContext(), render_target);
            return;
        }

        if (!render_framework)
            return;

        line_manager = CreateLineRenderManager(render_framework, render_target);
    }

    void LineRenderSystem::Render(graph::RenderCmdBuffer *cmd, float /*deltaTime*/)
    {
        EnsureLineManager();
        if (!cmd || !line_manager)
            return;

        line_manager->Draw(cmd);
    }
}//namespace hgl::ecs

