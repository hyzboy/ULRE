#pragma once
#include <hgl/ecs/support/pipeline/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * Issues the draw call for the SimpleLine buffer (single DrawMeshTasks).
     * Depends on SimpleLineBuildSystem (and indirectly RenderBufferUploadSystem)
     * having run first in the same frame.
     */
    class SimpleLineRenderSystem : public RenderPipelineDrawSystem
    {
    public:
        explicit SimpleLineRenderSystem(const AnsiString& name = "SimpleLineRenderSystem");
        ~SimpleLineRenderSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnRender(RenderPipelineBase* pipeline,
                      hgl::graph::RenderCmdBuffer* cmd) override;
    };

}  // namespace hgl::ecs
