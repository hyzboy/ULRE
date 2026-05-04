#include <hgl/ecs/support/terrain/TerrainRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/PipelineResolveMetrics.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/pipeline/VKRenderTargetFormat.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    namespace
    {
        PipelineHotpathCounters g_terrain_render_hotpath_counters;
    }

    const std::string TerrainRenderPipeline::kName{ "Terrain" };

    TerrainRenderPipeline::TerrainRenderPipeline(ECSContext* ctx)
        : context_(ctx)
    {}

    const std::string& TerrainRenderPipeline::GetName() const
    {
        return kName;
    }

    // ── Internal helpers ──────────────────────────────────────────────────────

    bool TerrainRenderPipeline::EnsureTileBuffer()
    {
        if (tile_buffer_initialized_)
            return true;

        auto* device = context_ ? context_->GetGPUDevice() : nullptr;
        auto* gc     = context_ ? context_->GetGraphicsContext() : nullptr;
        auto* bm     = gc ? gc->GetBufferManager() : nullptr;

        if (!device || !bm)
        {
            LogError("[TerrainRenderPipeline] GPU device or BufferManager not available");
            return false;
        }

        if (!tile_buffer_.Initialize(device, bm))
        {
            LogError("[TerrainRenderPipeline] Failed to initialize TerrainTileBuffer");
            return false;
        }

        tile_buffer_initialized_ = true;
        return true;
    }

    // ── RenderPipelineBase interface ──────────────────────────────────────────

    bool TerrainRenderPipeline::PrepareFrame()
    {
        visible_tiles_.clear();
        tile_buffer_.BeginFrame();
        return true;
    }

    void TerrainRenderPipeline::RunCollect()
    {
        if (!context_)
            return;

        std::vector<std::shared_ptr<TerrainTileComponent>> tiles;
        context_->GetComponents(tiles);

        for (const auto& tile : tiles)
        {
            if (!tile || !tile->visible)
                continue;
            if (!tile->material || !tile->pipeline)
                continue;
            if (tile->grid_width == 0 || tile->grid_height == 0)
                continue;

            visible_tiles_.push_back(tile.get());
        }
    }

    void TerrainRenderPipeline::RunBuild()
    {
        if (visible_tiles_.empty())
            return;

        if (!EnsureTileBuffer())
            return;

        for (const TerrainTileComponent* tile : visible_tiles_)
            tile_buffer_.AddTile(*tile);

        if (!tile_buffer_.Commit())
            LogError("[TerrainRenderPipeline] Commit failed; indirect draw will be skipped");
    }

    void TerrainRenderPipeline::Render(hgl::graph::RenderCmdBuffer* cmd)
    {
        if (!cmd || tile_buffer_.IsEmpty())
            return;

        const uint64_t vkcreate_before = graph::RenderTargetFormat::GetVkCreateCount();

        // All tiles share the same pipeline and material (first tile is representative)
        TerrainTileComponent* representative = visible_tiles_[0];

        // 1. Bind graphics pipeline
        cmd->BindPipeline(representative->pipeline);

        // 2. Bind material descriptor sets (also sets pipeline_layout on cmd, required for
        //    subsequent descriptor binding)
        cmd->BindDescriptorSets(representative->material);

        // 3. Bind instance-rate vertex buffer view.
        //    The terrain VS reads per-instance tile params from this buffer via gl_InstanceIndex.
        const auto* geom_data_buffer = tile_buffer_.GetGeometryDataBuffer();
        if (!geom_data_buffer || !cmd->BindDataBuffer(geom_data_buffer))
        {
            LogError("[TerrainRenderPipeline] Failed to bind tile geometry data buffer");
            return;
        }

        // 4. One vkCmdDrawIndirect for ALL tiles
        //    Each VkDrawIndirectCommand carries the per-tile vertex count (gw*gh*6).
        cmd->DrawIndirect(tile_buffer_.GetIndirectBuffer()->GetVkBuffer(),
                          tile_buffer_.GetTileCount());

        const uint64_t vkcreate_after = graph::RenderTargetFormat::GetVkCreateCount();
        const uint64_t vkcreate_delta = vkcreate_after - vkcreate_before;
        const uint64_t violation_log_count = RecordPipelineHotpathViolationAndGetLogCount(vkcreate_delta,
                                                                                           g_terrain_render_hotpath_counters);
        if (violation_log_count > 0)
        {
            GLogWarning("[TerrainRenderPipeline] Stage-4 violation: render hot path created %llu vk pipeline(s), total_violations=%llu",
                        static_cast<unsigned long long>(vkcreate_delta),
                        static_cast<unsigned long long>(violation_log_count));
        }
    }

}  // namespace hgl::ecs
