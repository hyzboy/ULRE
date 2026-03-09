#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * TerrainCollectSystem — 收集可见 TerrainTileComponent (RenderCollect 阶段)
     */
    class TerrainCollectSystem : public CollectSystem
    {
    public:
        explicit TerrainCollectSystem(const std::string& name = "TerrainCollectSystem");
        ~TerrainCollectSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnCollect(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
