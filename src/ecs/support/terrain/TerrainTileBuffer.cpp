#include <hgl/ecs/support/terrain/TerrainTileBuffer.h>
#include <hgl/ecs/components/TerrainTileComponent.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKVertexAttribBuffer.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/type/AlignUtil.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    bool TerrainTileBuffer::Initialize(hgl::graph::VulkanDevice* device, hgl::graph::BufferManager* bm)
    {
        if (!device || !bm)
            return false;
        device_         = device;
        buffer_manager_ = bm;
        return true;
    }

    // ── Per-frame data collection ─────────────────────────────────────────────

    void TerrainTileBuffer::BeginFrame()
    {
        tile_params_.clear();
        draw_cmds_.clear();
    }

    void TerrainTileBuffer::AddTile(const TerrainTileComponent& tile)
    {
        if (!tile.visible || tile.grid_width == 0 || tile.grid_height == 0)
            return;

        // Instance data: 4 ints packed into one ivec4 VAB attribute
        TerrainTileParams p;
        p.tile_x      = tile.tile_x;
        p.tile_y      = tile.tile_y;
        p.grid_width  = static_cast<int32_t>(tile.grid_width);
        p.grid_height = static_cast<int32_t>(tile.grid_height);
        tile_params_.push_back(p);

        // One indirect draw command per tile
        VkDrawIndirectCommand cmd;
        cmd.vertexCount   = tile.VertexCount();                                   // gw * gh * 6
        cmd.instanceCount = 1;
        cmd.firstVertex   = 0;
        cmd.firstInstance = static_cast<uint32_t>(tile_params_.size() - 1);       // → gl_InstanceIndex in VS
        draw_cmds_.push_back(cmd);
    }

    // ── GPU buffer management ─────────────────────────────────────────────────

    bool TerrainTileBuffer::ReallocVAB(uint32_t required_count)
    {
        const uint32_t new_cap = static_cast<uint32_t>(hgl::power_to_2(required_count));
        if (new_cap <= vab_capacity_)
            return true;

        // Previous VAB is released by the device resource manager when no longer referenced.
        // We simply let go of the pointer and create a new, larger buffer.
        tile_vab_     = nullptr;
        tile_vab_buf_ = VK_NULL_HANDLE;

        tile_vab_ = buffer_manager_->CreateVAB(
            VK_FORMAT_R32G32B32A32_SINT,
            new_cap,
            nullptr,                             // no initial data; written in Commit()
            hgl::graph::BufferAllocPolicy::Auto);

        if (!tile_vab_)
        {
            LogError("[TerrainTileBuffer] Failed to allocate instance VAB (capacity=%u)", new_cap);
            vab_capacity_ = 0;
            return false;
        }

        tile_vab_buf_ = tile_vab_->GetVkBuffer();
        vab_capacity_ = new_cap;
        return true;
    }

    bool TerrainTileBuffer::ReallocIndirect(uint32_t required_count)
    {
        const uint32_t new_cap = static_cast<uint32_t>(hgl::power_to_2(required_count));
        if (new_cap <= icb_capacity_)
            return true;

        indirect_buf_ = nullptr;
        indirect_buf_ = device_->CreateIndirectDrawBuffer(
            new_cap,
            hgl::graph::BufferAllocPolicy::Auto,
            "TerrainTileIndirect");

        if (!indirect_buf_)
        {
            LogError("[TerrainTileBuffer] Failed to allocate indirect draw buffer (capacity=%u)", new_cap);
            icb_capacity_ = 0;
            return false;
        }

        icb_capacity_ = new_cap;
        return true;
    }

    bool TerrainTileBuffer::Commit()
    {
        const uint32_t count = GetTileCount();
        if (count == 0)
            return true;

        if (!ReallocVAB(count) || !ReallocIndirect(count))
            return false;

        // Write instance params to VAB (start=0, size=count elements)
        if (!tile_vab_->Write(tile_params_.data(), 0, count))
        {
            LogError("[TerrainTileBuffer] Failed to write tile params to instance VAB");
            return false;
        }

        // Write indirect draw commands
        if (!indirect_buf_->WriteCmd(draw_cmds_.data(), count))
        {
            LogError("[TerrainTileBuffer] Failed to write indirect draw commands");
            return false;
        }

        return true;
    }

}  // namespace hgl::ecs
