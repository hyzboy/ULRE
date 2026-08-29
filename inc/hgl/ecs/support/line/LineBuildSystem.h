#pragma once
#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * Runs the build pass for LineRenderPipeline.
     * Counts lines per width-slot, (re)allocates GPU buffers as needed,
     * then writes all segments in a second pass.
     */
    class LineBuildSystem : public BuildSystem
    {
    public:
        explicit LineBuildSystem(const std::string& name = "LineBuildSystem");
        ~LineBuildSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnBuild(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
