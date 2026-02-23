#pragma once

#include <hgl/ecs/support/RenderPipelineGroup.h>

namespace hgl::ecs
{
    /**
     * TextRenderPipelineGroup - Container for the Text render pipeline and its systems
     *
     * Registers TextRenderPipelineAdapter + 4 systems (Collect/Build/Sync/Render)
     * into ECSContext, replacing the old TextCollectSystem / TextBuildSystem /
     * TextResourceSyncSystem / TextRenderSubmitSystem.
     */
    class TextRenderPipelineGroup : public RenderPipelineGroup
    {
    public:
        TextRenderPipelineGroup();
        ~TextRenderPipelineGroup() override = default;

        bool Initialize(ECSContext* context) override;
        void Shutdown(ECSContext* context) override;

    protected:
        std::unique_ptr<RenderPipelineBase> CreatePipeline() override;
        void RegisterSystems() override;
    };

}  // namespace hgl::ecs
