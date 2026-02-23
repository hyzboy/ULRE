#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * TextRenderSystem - RenderDrawSubmit phase for Text elements
     *
     * Records GPU draw commands for all visible text primitives.
     * Replaces the old TextRenderSubmitSystem.
     */
    class TextRenderSystem : public RenderPipelineDrawSystem
    {
    public:
        explicit TextRenderSystem(const std::string& name = "TextRenderSystem");
        ~TextRenderSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd) override;
    };

}  // namespace hgl::ecs
