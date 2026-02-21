#include<hgl/ecs/systems/render/TextResourceSyncSystem.h>
#include<hgl/ecs/systems/render/TextBuildSystem.h>
#include<hgl/ecs/support/TextRenderPipeline.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TextResourceSyncSystem::TextResourceSyncSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch_TextResourceSyncSystem);
        SetRenderElementType("Text");
        AddDependency<TextBuildSystem>();
    }

    void TextResourceSyncSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto pipeline = context->GetTextRenderPipeline();
        if (!pipeline)
            return;

        if (!pipeline->PrepareFrame())
            return;

        pipeline->RunSync();
    }
}
