#pragma once

#include <hgl/ecs/support/RenderPipelineBase.h>
#include <hgl/ecs/components/TerrainTileComponent.h>
#include <hgl/ecs/support/terrain/TerrainTileBuffer.h>
#include <hgl/log/Log.h>
#include <memory>
#include <vector>
#include <string>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * TerrainRenderPipeline — Indirect + Instance VAB 地形渲染管线
     *
     * 每帧执行阶段：
     *   PrepareFrame()  — 清空 visible_tiles_ 与 tile_buffer_
     *   RunCollect()    — 收集可见 TerrainTileComponent（RenderCollect 阶段）
     *   RunBuild()      — AddTile × N → Commit()，写入 GPU VAB + IndirectDrawBuffer（RenderBatch 阶段）
     *   Render(cmd)     — BindPipeline → BindDescriptorSets → BindVAB → vkCmdDrawIndirect（RenderDrawSubmit 阶段）
     *
     * 关键约束：场景中所有 Tile 必须共享同一 pipeline 与 material，
     *            才能合并为一次 vkCmdDrawIndirect。
     */
    class TerrainRenderPipeline : public RenderPipelineBase
    {
        OBJECT_LOGGER

    public:
        static const std::string kName;  ///< "Terrain"

    private:
        ECSContext* context_ = nullptr;

        // 每帧可见 Tile（弱引用，生命周期由 ECS 管理）
        std::vector<TerrainTileComponent*> visible_tiles_;

        // Instance VAB + IndirectDrawBuffer 管理器
        TerrainTileBuffer tile_buffer_;
        bool tile_buffer_initialized_ = false;

        bool EnsureTileBuffer();

    public:
        explicit TerrainRenderPipeline(ECSContext* ctx);
        ~TerrainRenderPipeline() override = default;

        // ── RenderPipelineBase interface ──────────────────────────

        const std::string& GetName() const override;
        ECSContext*         GetWorld() const override { return context_; }

        bool PrepareFrame() override;
        void RunCollect()   override;
        void RunBuild()     override;    ///< 写入 GPU 缓冲（RenderBatch 阶段）
        void Render(hgl::graph::RenderCmdBuffer* cmd) override;

        void RunSync() override {}

        // ── Accessors ────────────────────────────────────────────

        bool HasVisibleTiles() const { return !visible_tiles_.empty(); }

        /// 无 Primitive 对象（程序化地形）
        void GetRenderPrimitives(std::vector<hgl::graph::Primitive*>& /*out*/) const override {}
    };

}  // namespace hgl::ecs
