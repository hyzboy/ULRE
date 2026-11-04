#pragma once
#include <hgl/ecs/support/pipeline/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * Runs the build pass for SimpleLineRenderPipeline.
     * Counts lines, (re)allocates GPU buffers as needed,
     * then writes all segments in a second pass.
     */
    class SimpleLineBuildSystem : public BuildSystem
    {
    public:
        explicit SimpleLineBuildSystem(const AnsiString& name = "SimpleLineBuildSystem");
        ~SimpleLineBuildSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnBuild(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
