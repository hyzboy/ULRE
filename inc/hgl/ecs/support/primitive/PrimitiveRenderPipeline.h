#pragma once

#include <hgl/ecs/support/RenderPipelineBase.h>
#include <hgl/ecs/support/PrimitiveBatchPipeline.h>
#include <memory>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * PrimitiveRenderPipeline - RenderPipelineBase adapter for PrimitiveBatchPipeline
     *
     * This class wraps the existing PrimitiveBatchPipeline to conform to RenderPipelineBase interface.
     * It delegates all pipeline operations to the underlying PrimitiveBatchPipeline instance.
     *
     * All rendering logic remains in PrimitiveBatchPipeline - this class just provides
     * the unified interface that the ECS framework expects.
     */
    class PrimitiveRenderPipeline : public RenderPipelineBase
    {
    private:
        std::unique_ptr<PrimitiveBatchPipeline> impl_;
        ECSContext* context_ = nullptr;

    public:
        explicit PrimitiveRenderPipeline(ECSContext* context);
        ~PrimitiveRenderPipeline() override = default;

        const std::string& GetName() const override;
        ECSContext* GetWorld() const override;

        bool PrepareFrame() override;
        void RunCollect() override {}    // Not used for Primitive
        void RunCull() override;
        void RunSort() override;
        void RunBuild() override;
        void RunSync() override {}       // Not used for Primitive
        void GetRenderPrimitives(std::vector<hgl::graph::Primitive*>& out_primitives) const override;
        void Render(hgl::graph::RenderCmdBuffer* cmd) override;
        void Shutdown() override;

        // Access to underlying pipeline for system integration
        PrimitiveBatchPipeline* GetImplementation() const { return impl_.get(); }
    };

}  // namespace hgl::ecs
