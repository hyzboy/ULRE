#include<hgl/ecs/LineRenderSystem.h>
#include<hgl/ecs/RenderPrimitiveSubmitSystem.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/graph/VKCommandBuffer.h>

namespace hgl::ecs
{
    LineRenderSystem::LineRenderSystem(const std::string &name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(130);
        AddDependency<RenderPrimitiveSubmitSystem>();
    }

    void LineRenderSystem::Render(graph::RenderCmdBuffer *cmd, float /*deltaTime*/)
    {
        if (!cmd || !line_manager)
            return;

        line_manager->Draw(cmd);
    }
}//namespace hgl::ecs
