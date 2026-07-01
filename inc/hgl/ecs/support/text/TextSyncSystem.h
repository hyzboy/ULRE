#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
        * TextSyncSystem - RenderFrameSync phase for Text elements
     *
        * Calls pipeline->RunSync() to synchronize GPU resources after upload.
     * Replaces the old TextResourceSyncSystem.
     */
    class TextSyncSystem : public SyncSystem
    {
    public:
        explicit TextSyncSystem(const std::string& name = "TextSyncSystem");
        ~TextSyncSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnSync(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
