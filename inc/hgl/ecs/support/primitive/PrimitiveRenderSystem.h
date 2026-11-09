#pragma once

#include <hgl/ecs/support/pipeline/RenderPipelineSystem.h>
#include <hgl/mtl/RenderLayer.h>

namespace hgl::ecs
{
    /**
     * PrimitiveRenderSystem - RenderDrawSubmit phase for primitives
     *
     * Records GPU draw commands for all primitive material batches.
     * Derived from RenderPipelineDrawSystem to provide unified System interface.
     */
    class PrimitiveRenderSystem : public RenderPipelineDrawSystem
    {
    public:
        explicit PrimitiveRenderSystem(const std::string& name = "PrimitiveRenderSystem");
        ~PrimitiveRenderSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

        /// 统一批次渲染核心实现（按固定渲染分段过滤）
        static void RenderBatches(ECSContext* context, hgl::graph::RenderCmdBuffer* cmd, hgl::graph::mtl::RenderLayer layer);

    private:
        void OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd) override;
    };

}  // namespace hgl::ecs
