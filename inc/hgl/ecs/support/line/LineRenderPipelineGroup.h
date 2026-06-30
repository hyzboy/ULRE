#pragma once
#include <hgl/ecs/support/RenderPipelineGroup.h>

namespace hgl::ecs
{
    /**
     * Self-contained group that owns the LineRenderPipeline and wires up
     * LineCollectSystem → LineBuildSystem → LineRenderSystem.
     *
     * Registered via InstallLineGroup in DefaultSystems.cpp.
     */
    class LineRenderPipelineGroup : public RenderPipelineGroup
    {
    public:
        LineRenderPipelineGroup();
        ~LineRenderPipelineGroup() override = default;

        bool Initialize(ECSContext* context) override;
        void Shutdown(ECSContext* context) override;
    };

}  // namespace hgl::ecs
