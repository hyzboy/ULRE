#include<hgl/ecs/systems/render/TextCollectSystem.h>
#include<hgl/ecs/support/TextRenderPipeline.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TextCollectSystem::TextCollectSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect_TextCollectSystem);
        SetRenderElementType("Text");
        AddDependency<RenderPrimitiveCollectSystem>();
    }

    void TextCollectSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto pipeline = context->GetTextRenderPipeline();
        if (!pipeline)
            return;

        if (!pipeline->PrepareFrame())
            return;

        pipeline->RunCollect();
    }
}
