#include <hgl/ecs/support/text/TextRenderPipelineAdapter.h>
#include <hgl/ecs/support/TextRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/vk/VKCommandBuffer.h>

namespace hgl::ecs
{
    const std::string TextRenderPipelineAdapter::kName{ "Text" };

    TextRenderPipelineAdapter::TextRenderPipelineAdapter(ECSContext* context)
        : context_(context)
        , impl_(std::make_unique<TextRenderPipeline>())
    {
        impl_->SetWorld(context_);
        impl_->SetRenderContext(context_->GetRenderContext());
    }

    const std::string& TextRenderPipelineAdapter::GetName() const
    {
        return kName;
    }

    ECSContext* TextRenderPipelineAdapter::GetWorld() const
    {
        return context_;
    }

    bool TextRenderPipelineAdapter::PrepareFrame()
    {
        return impl_->PrepareFrame();
    }

    void TextRenderPipelineAdapter::RunCollect()
    {
        if (!impl_->PrepareFrame())
            return;
        impl_->RunCollect();
    }

    void TextRenderPipelineAdapter::RunBuild()
    {
        if (!impl_->PrepareFrame())
            return;
        impl_->RunBuild();
    }

    void TextRenderPipelineAdapter::RunSync()
    {
        if (!impl_->PrepareFrame())
            return;
        impl_->RunSync();
    }

    void TextRenderPipelineAdapter::GetRenderPrimitives(std::vector<hgl::graph::Primitive*>& out) const
    {
        impl_->GetRenderPrimitives(out);
    }

    void TextRenderPipelineAdapter::Render(hgl::graph::RenderCmdBuffer* cmd)
    {
        if (!cmd)
            return;

        std::vector<TextRenderPipeline::RenderEntry> entries;
        impl_->GetRenderEntries(entries);

        for (const auto& entry : entries)
        {
            auto* primitive = entry.first;
            auto* pipeline = entry.second;
            if (primitive && pipeline)
                cmd->Render(primitive, pipeline);
        }
    }

}  // namespace hgl::ecs
