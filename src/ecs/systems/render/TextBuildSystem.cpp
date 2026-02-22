#include<hgl/ecs/systems/render/TextBuildSystem.h>
#include<hgl/ecs/systems/render/TextCollectSystem.h>
#include<hgl/ecs/support/TextRenderPipeline.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TextBuildSystem::TextBuildSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        SetRenderElementType("Text");
        AddDependency<TextCollectSystem>();
    }

    void TextBuildSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto pipeline = context->GetTextRenderPipeline();
        if (!pipeline)
            return;

        if (!pipeline->PrepareFrame())
            return;

        pipeline->RunBuild();
    }
}
