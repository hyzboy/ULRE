#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * TerrainRenderSystem — 录制地形 Tile 绘制命令 (RenderDrawSubmit 阶段)
     *
     * 调用 TerrainRenderPipeline::Render(cmd)，后者对每个可见 Tile 执行：
     *   BindPipeline → BindDescriptorSets → PushConstants → vkCmdDraw
     * 无任何 VBO/IBO 绑定。
     */
    class TerrainRenderSystem : public RenderPipelineDrawSystem
    {
    public:
        explicit TerrainRenderSystem(const std::string& name = "TerrainRenderSystem");
        ~TerrainRenderSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnRender(RenderPipelineBase* pipeline,
                      hgl::graph::RenderCmdBuffer* cmd) override;
    };

}  // namespace hgl::ecs
