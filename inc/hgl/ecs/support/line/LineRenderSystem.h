#pragma once
#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * Issues draw calls for all visible line slots.
     * Depends on LineBuildSystem (and indirectly RenderBufferUploadSystem)
     * having run first in the same frame.
     */
    class LineRenderSystem : public RenderPipelineDrawSystem
    {
    public:
        explicit LineRenderSystem(const std::string& name = "LineRenderSystem");
        ~LineRenderSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnRender(RenderPipelineBase* pipeline,
                      hgl::graph::RenderCmdBuffer* cmd) override;
    };

}  // namespace hgl::ecs
