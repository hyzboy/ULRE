#pragma once

#include <hgl/ecs/support/pipeline/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * TextOverlayRenderSystem - RenderOverlay phase for Text elements
     *
     * 提交 Overlay 渲染分段的文本（DEBUG PROF 等最终覆盖层），在所有场景/
     * Gizmo/2D UI 文本之后绘制。与 TextRenderSystem（UI2D）共用同一条
     * "Text" 管线，仅以 RenderLayer::Overlay 过滤提交。
     */
    class TextOverlayRenderSystem : public RenderPipelineDrawSystem
    {
    public:
        explicit TextOverlayRenderSystem(const std::string& name = "TextOverlayRenderSystem");
        ~TextOverlayRenderSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd) override;
    };

}  // namespace hgl::ecs
