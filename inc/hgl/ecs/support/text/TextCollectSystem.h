#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * TextCollectSystem - RenderCollect phase for Text elements
     *
     * Calls pipeline->RunCollect() to gather visible TextComponents.
     */
    class TextCollectSystem : public CollectSystem
    {
    public:
        explicit TextCollectSystem(const std::string& name = "TextCollectSystem");
        ~TextCollectSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnCollect(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
