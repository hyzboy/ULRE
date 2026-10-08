#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

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

        /// 统一批次渲染核心实现（支持普通/Overlay过滤）
        static void RenderBatches(ECSContext* context, hgl::graph::RenderCmdBuffer* cmd, bool overlay_only);

    private:
        void OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd) override;
    };

}  // namespace hgl::ecs
