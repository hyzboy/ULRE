#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * PrimitiveBuildSystem - RenderBatch phase for primitives
     *
     * Builds material batches and writes to vertex/index buffers.
     * Derived from BuildSystem to provide unified System interface.
     */
    class PrimitiveBuildSystem : public BuildSystem
    {
    public:
        explicit PrimitiveBuildSystem(const std::string& name = "PrimitiveBuildSystem");
        ~PrimitiveBuildSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnBuild(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
