#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * TerrainBuildSystem — 构建 Instance VAB 与 Indirect Draw Buffer（RenderBatch 阶段）
     *
     * 执行顺序：TerrainCollectSystem → TerrainBuildSystem → TerrainRenderSystem
     *
     * 职责：
     *   将当前帧 TerrainCollectSystem 收集的可见 Tile 列表写入 GPU 缓冲：
     *     - Instance-rate VAB：(tile_x, tile_y, grid_w, grid_h) × N tiles
     *     - IndirectDrawBuffer：VkDrawIndirectCommand × N tiles
     *
     * 调用 TerrainRenderPipeline::RunBuild()。
     */
    class TerrainBuildSystem : public BuildSystem
    {
    public:
        explicit TerrainBuildSystem(const std::string& name = "TerrainBuildSystem");
        ~TerrainBuildSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnBuild(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
