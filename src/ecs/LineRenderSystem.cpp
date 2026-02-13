#include<hgl/ecs/LineRenderSystem.h>
#include<hgl/ecs/RenderPrimitiveSubmitSystem.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKRenderTarget.h>

namespace hgl::graph
{
    LineRenderManager *CreateLineRenderManager(RenderFramework *, IRenderTarget *);
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
        if (line_manager || !render_framework || !render_target)
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
