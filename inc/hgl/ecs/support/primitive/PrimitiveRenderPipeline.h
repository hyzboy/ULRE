#pragma once

#include <hgl/ecs/support/pipeline/RenderPipelineBase.h>
#include <hgl/ecs/support/pipeline/RenderBatchPipeline.h>
#include <memory>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * PrimitiveRenderPipeline - RenderPipelineBase adapter for RenderBatchPipeline
     *
     * This class wraps the existing RenderBatchPipeline to conform to RenderPipelineBase interface.
     * It delegates all pipeline operations to the underlying RenderBatchPipeline instance.
     *
     * All rendering logic remains in RenderBatchPipeline - this class just provides
     * the unified interface that the ECS framework expects.
     */
    class PrimitiveRenderPipeline : public RenderPipelineBase
    {
    private:
        // W7 统一：类静态成员 kName（Line/Text/Primitive 三管线同模式）
        static const AnsiString kName;
        std::unique_ptr<RenderBatchPipeline> impl_;
        ECSContext* context_ = nullptr;

    public:
        explicit PrimitiveRenderPipeline(ECSContext* context);
        ~PrimitiveRenderPipeline() override = default;

        const AnsiString& GetName() const override;
        ECSContext* GetWorld() const override;

        bool PrepareFrame() override;
        void RunCollect() override {}    // Not used for Primitive
        void RunCull() override;
        void RunSort() override;
        void RunBuild() override;
        void RunSync() override {}       // Not used for Primitive
        void Render(hgl::graph::RenderCmdBuffer* cmd) override;
        void Shutdown() override;

        // Access to underlying pipeline for system integration
        RenderBatchPipeline* GetImplementation() const { return impl_.get(); }
    };

}  // namespace hgl::ecs
