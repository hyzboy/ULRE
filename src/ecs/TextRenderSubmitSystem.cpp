#include<hgl/ecs/TextRenderSubmitSystem.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/TextRenderSystem.h>
#include<hgl/ecs/RenderPrimitiveSubmitSystem.h>
#include<hgl/graph/VKCommandBuffer.h>

namespace hgl::ecs
{
    TextRenderSubmitSystem::TextRenderSubmitSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderPostProcess, ExecutionPriority::First);
        AddDependency<RenderPrimitiveSubmitSystem>();
    }

    void TextRenderSubmitSystem::Render(graph::RenderCmdBuffer* cmdBuffer, float /*deltaTime*/)
    {
        if (!world || !cmdBuffer)
            return;

        auto text_render_sys = world->GetSystem<TextRenderSystem>();
        if (!text_render_sys)
            return;

        std::vector<graph::Primitive*> primitives;
        text_render_sys->GetRenderPrimitives(primitives);

        for (auto* primitive : primitives)
        {
            if (primitive)
                cmdBuffer->Render(primitive);
        }
    }
}//namespace hgl::ecs
