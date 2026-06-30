#include <hgl/ecs/support/text/TextRenderPipelineGroup.h>
#include <hgl/ecs/support/text/TextRenderPipelineAdapter.h>
#include <hgl/ecs/support/text/TextCollectSystem.h>
#include <hgl/ecs/support/text/TextBuildSystem.h>
#include <hgl/ecs/support/text/TextSyncSystem.h>
#include <hgl/ecs/support/text/TextRenderSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TextRenderPipelineGroup::TextRenderPipelineGroup()
        : RenderPipelineGroup("Text")
    {
    }

    bool TextRenderPipelineGroup::Initialize(ECSContext* context)
    {
        // 1. Create adapter wrapping Context's existing TextRenderPipeline
        auto adapter = std::make_unique<TextRenderPipelineAdapter>(context);

        // 2. Register adapter to Context under name "Text"
        context->RegisterRenderPipeline(name_, std::move(adapter));

        // 3. Register pipeline-driven systems (Context sets their context pointer automatically)
        context->RegisterRenderSystem<TextCollectSystem>();
        context->RegisterRenderSystem<TextBuildSystem>();
        context->RegisterRenderSystem<TextSyncSystem>();
        context->RegisterRenderSystem<TextRenderSystem>();

        return true;
    }

    void TextRenderPipelineGroup::Shutdown(ECSContext* /*context*/)
    {
        systems_.clear();
        pipeline_.reset();
    }

}  // namespace hgl::ecs
