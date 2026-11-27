#pragma once

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/cull/GPUCullSystem.h>
#include <hgl/ecs/systems/render/collect/RenderCollectSystem.h>
#include <hgl/graph/asset/MaterialDataBlockAsset.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/vk/buffer/IGPUBuffer.h>
#include <hgl/vk/buffer/IndirectCommandBuffer.h>
#include <hgl/vk/VKBufferReadback.h>
#include <hgl/log/Log.h>

namespace ulre_example
{
    enum class InstancingGroups { Single, PipelineHalves, MeshColumns };

    inline bool CheckInstancingRows(hgl::ecs::ECSContext *world,
                                    const hgl::graph::mtl::MaterialDataBlockAsset *data_block,
                                    InstancingGroups grouping, const char *tag)
    {
        constexpr uint32_t grid_size = 10u;
        constexpr uint32_t row_count = grid_size * grid_size;
        auto cull = world->GetSystem<hgl::ecs::GPUCullSystem>();
        auto collect = world->GetSystem<hgl::ecs::RenderCollectSystem>();
        if (!cull || !collect || !data_block || !cull->SyncForParityReadback())
        {
            GLogError(u8"[%s] GPU readback unavailable", tag);
            return false;
        }
        const uint32_t buckets = cull->GetBatchCount();
        const uint32_t *counts = cull->MapCurrentBatchCounters(0u);
        if (!counts)
        {
            GLogError(u8"[%s] GPU counters unavailable", tag);
            return false;
        }
        const uint32_t expected_buckets = grouping == InstancingGroups::Single ? 1u
            : grouping == InstancingGroups::PipelineHalves ? 2u : grid_size;
        bool ok = buckets == expected_buckets && counts[buckets] == 0u;
        bool seen[row_count]{};
        bool seen_group[grid_size]{};
        uint32_t bucket_counts[grid_size]{};
        uint32_t total = 0;
        auto *buffer = cull->GetMtlOutBuffer();
        for (uint32_t bucket = 0; bucket < buckets; ++bucket)
        {
            uint32_t base = 0, capacity = 0;
            const uint32_t count = counts[bucket];
            if (bucket < grid_size)
                bucket_counts[bucket] = count;
            if (!cull->GetBatchLayout(0u, bucket, base, capacity) || count > capacity
             || !count || !buffer || !buffer->GetGPUBuffer())
            {
                ok = false;
                break;
            }
            const auto *rows = static_cast<const hgl::graph::mtl::MaterialInstanceAddresses *>(
                buffer->GetGPUBuffer()->Map(uint64_t(base) * sizeof(hgl::graph::mtl::MaterialInstanceAddresses),
                                            uint64_t(count) * sizeof(hgl::graph::mtl::MaterialInstanceAddresses)));
            if (!rows)
            {
                ok = false;
                break;
            }
            ok &= count == row_count / expected_buckets;
            int bucket_group = -1;
            for (uint32_t i = 0; i < count; ++i)
            {
                bool found = false;
                for (uint32_t j = 0; j < row_count; ++j)
                    if (rows[i].payload_index == data_block->RowIDAt(j))
                    {
                        found = !seen[j];
                        seen[j] = true;
                        const int group = grouping == InstancingGroups::Single ? 0
                            : grouping == InstancingGroups::PipelineHalves ? int(j / 50u)
                            : int(j % grid_size);
                        if (bucket_group < 0)
                            bucket_group = group;
                        else
                            ok &= bucket_group == group;
                        break;
                    }
                ok &= found;
            }
            buffer->GetGPUBuffer()->Unmap();
            if (bucket_group >= 0)
            {
                ok &= !seen_group[bucket_group];
                seen_group[bucket_group] = true;
            }
            total += count;
            GLogInfo(u8"[%s] GPU bucket=%u instances=%u capacity=%u", tag, bucket, count, capacity);
        }
        cull->UnmapCurrentBatchCounters();
        VkDrawMeshTasksIndirectCommandEXT commands[grid_size]{};
        auto *indirect = cull->GetIndirectCommandsBuffer();
        if (buckets != expected_buckets || !indirect
         || !hgl::graph::ReadbackBuffer(world->GetGPUDevice(), indirect->GetVkBuffer(), 0,
                                        sizeof(commands[0]) * expected_buckets, commands))
            ok = false;
        else
            for (uint32_t bucket = 0; bucket < expected_buckets; ++bucket)
                ok &= commands[bucket].groupCountX > 0u
                   && commands[bucket].groupCountY == bucket_counts[bucket]
                   && commands[bucket].groupCountZ == 1u;
        ok &= total == row_count;
        for (uint32_t g = 0; g < expected_buckets; ++g)
            ok &= seen_group[g];
        const uint64_t hits = collect->GetCollectCacheHits();
        ok &= !world->Runner().GetMainFrameTopology().IsCollectCacheEnabled() || hits > 0u;
        GLogInfo(u8"[%s] GPU buckets=%u instances=%u cache_hits=%llu",
                 tag, buckets, total, static_cast<unsigned long long>(hits));
        if (!ok)
            GLogError(u8"[%s] GPU instance row validation failed", tag);
        return ok;
    }
}
