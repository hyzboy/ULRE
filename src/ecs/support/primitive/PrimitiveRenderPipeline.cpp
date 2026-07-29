#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    static const std::string kPipelineName{ "Primitive" };

    PrimitiveRenderPipeline::PrimitiveRenderPipeline(ECSContext* context)
        : context_(context)
    {
        impl_ = std::make_unique<PrimitiveBatchPipeline>();
    }

    const std::string& PrimitiveRenderPipeline::GetName() const
    {
        return kPipelineName;
    }

    ECSContext* PrimitiveRenderPipeline::GetWorld() const
    {
        return context_;
    }

    bool PrimitiveRenderPipeline::PrepareFrame()
    {
        return impl_->PrepareFrame(context_);
    }

    void PrimitiveRenderPipeline::RunCull()
    {
        if (!impl_->PrepareFrame(context_))
            return;
        impl_->RunCulling();
    }

    void PrimitiveRenderPipeline::RunSort()
    {
        if (!impl_->PrepareFrame(context_))
            return;
        impl_->RunSorting();
    }

    void PrimitiveRenderPipeline::RunBuild()
    {
        if (!impl_->PrepareFrame(context_))
            return;
        impl_->RunTransformIndexing();
        impl_->RunBatching();
    }

    void PrimitiveRenderPipeline::Render(hgl::graph::RenderCmdBuffer* /*cmd*/)
    {
        // Draw commands are recorded by PrimitiveRenderSystem which reads from
        // context's render_frame_cache directly (via PipelineMaterialRenderer).
        // This method intentionally left for future use.
    }

    void PrimitiveRenderPipeline::Shutdown()
    {
        impl_.reset();
        context_ = nullptr;
    }

}  // namespace hgl::ecs
