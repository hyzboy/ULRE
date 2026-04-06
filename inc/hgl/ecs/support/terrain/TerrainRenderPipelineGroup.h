#pragma once

#include <hgl/ecs/support/RenderPipelineGroup.h>

namespace hgl::ecs
{
    /**
     * TerrainRenderPipelineGroup — 地形无 VBO/IBO 渲染管线组
     *
     * 注册内容：
     *   - TerrainRenderPipeline  → Context 管线注册表 ("Terrain")
     *   - TerrainCollectSystem   → RenderCollect 阶段
     *   - TerrainRenderSystem    → RenderDrawSubmit 阶段
     *
     * 用法：
     *   TerrainRenderPipelineGroup group;
     *   group.Initialize(context);
     */
    class TerrainRenderPipelineGroup : public RenderPipelineGroup
    {
    public:
        TerrainRenderPipelineGroup();
        ~TerrainRenderPipelineGroup() override = default;

        bool Initialize(ECSContext* context) override;
        void Shutdown(ECSContext* context) override;

    protected:
        std::unique_ptr<RenderPipelineBase> CreatePipeline() override;
        void RegisterSystems() override;
    };

}  // namespace hgl::ecs
