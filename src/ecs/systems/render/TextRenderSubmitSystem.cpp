#include<hgl/ecs/systems/render/TextRenderSubmitSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/TextResourceSyncSystem.h>
#include<hgl/ecs/support/TextRenderPipeline.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/vk/VKCommandBuffer.h>

namespace hgl::ecs
{
    TextRenderSubmitSystem::TextRenderSubmitSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderDrawSubmit);
        SetRenderElementType("Text");
        AddDependency<TextResourceSyncSystem>();
        AddDependency<RenderPrimitiveSubmitSystem>();
        AddDependency<RenderBufferUploadSystem>();
    }

    void TextRenderSubmitSystem::Render(graph::RenderCmdBuffer* cmdBuffer, float /*deltaTime*/)
    {
        if (!world || !cmdBuffer)
            return;

        auto text_pipeline = world->GetTextRenderPipeline();
        if (!text_pipeline)
            return;

        std::vector<graph::Primitive*> primitives;
        text_pipeline->GetRenderPrimitives(primitives);

        for (auto* primitive : primitives)
        {
            if (primitive)
                cmdBuffer->Render(primitive);
        }
    }
}//namespace hgl::ecs

