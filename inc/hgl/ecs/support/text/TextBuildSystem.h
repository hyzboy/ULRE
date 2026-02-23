#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * TextBuildSystem - RenderBatch phase for Text elements
     *
     * Calls pipeline->RunBuild() to build text geometry and write GPU buffers.
     */
    class TextBuildSystem : public BuildSystem
    {
    public:
        explicit TextBuildSystem(const std::string& name = "TextBuildSystem");
        ~TextBuildSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnBuild(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
