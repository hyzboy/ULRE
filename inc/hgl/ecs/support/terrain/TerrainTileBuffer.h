#pragma once

#include <hgl/vk/VKIndirectCommandBuffer.h>
#include <hgl/log/Log.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace hgl::graph
{
    class VulkanDevice;
    class BufferManager;
    class VertexAttribBuffer;
    using VAB = VertexAttribBuffer;
}

namespace hgl::ecs
{
    struct TerrainTileComponent;

    /**
     * TerrainTileParams — 每个 Tile 存入 Instance-rate VAB 的数据
     *
     * GLSL 对应声明：
     *   layout(location = 0) in ivec4 i_tile;  // (tile_x, tile_y, grid_w, grid_h)
     *
     * 格式：VK_FORMAT_R32G32B32A32_SINT，stride = 16 字节，VK_VERTEX_INPUT_RATE_INSTANCE
     */
    struct TerrainTileParams
    {
        int32_t tile_x;
        int32_t tile_y;
        int32_t grid_width;   ///< 以 int32 存储，shader 中用 uint(i_tile.z) 取用
        int32_t grid_height;
    };
    static_assert(sizeof(TerrainTileParams) == 16, "TerrainTileParams must be 16 bytes");

    /**
     * TerrainTileBuffer — 管理所有可见 Tile 的 Instance VAB 与 Indirect Draw Buffer
     *
     * 每帧工作流程：
     *   1. BeginFrame()         — 清空 CPU staging 向量
     *   2. AddTile(component)   — 追加 Tile 参数 + VkDrawIndirectCommand
     *   3. Commit()             — 按需扩容 GPU 缓冲，写入数据
     *   4. 在 Render pass 中：
     *      a. BindVAB(cmd)      — 绑定 instance-rate VAB 到 binding 0
     *      b. DrawIndirect(cmd) — 一次 vkCmdDrawIndirect 绘制全部 Tile
     *
     * 参考：TransformAssignmentBuffer（Instance VAB 管理模式）
     */
    class TerrainTileBuffer
    {
        OBJECT_LOGGER

        hgl::graph::VulkanDevice*  device_         = nullptr;
        hgl::graph::BufferManager* buffer_manager_ = nullptr;

        // CPU staging（每帧重建）
        std::vector<TerrainTileParams>      tile_params_;
        std::vector<VkDrawIndirectCommand>  draw_cmds_;

        // GPU：instance-rate VAB（VK_FORMAT_R32G32B32A32_SINT，stride=16）
        hgl::graph::VAB* tile_vab_     = nullptr;
        VkBuffer         tile_vab_buf_ = VK_NULL_HANDLE;
        uint32_t         vab_capacity_ = 0;

        // GPU：Indirect draw 命令缓冲
        hgl::graph::IndirectDrawBuffer* indirect_buf_ = nullptr;
        uint32_t                        icb_capacity_ = 0;

        bool ReallocVAB     (uint32_t required_count);
        bool ReallocIndirect(uint32_t required_count);

    public:
        TerrainTileBuffer() = default;
        ~TerrainTileBuffer() = default;  ///< VAB / ICB 由 VulkanDevice 所有

        /**
         * 初始化（必须在使用前调用）
         * @param device  Vulkan 设备（创建 IndirectDrawBuffer）
         * @param bm      BufferManager（创建 VAB）
         */
        bool Initialize(hgl::graph::VulkanDevice* device, hgl::graph::BufferManager* bm);

        void BeginFrame();                                ///< 清空 CPU staging，开始新帧
        void AddTile(const TerrainTileComponent& tile);  ///< 追加一个 Tile 的实例数据 + draw cmd
        bool Commit();                                    ///< 将 CPU 数据写入（并按需扩容） GPU 缓冲

        bool     IsEmpty()      const { return tile_params_.empty(); }
        uint32_t GetTileCount() const { return static_cast<uint32_t>(tile_params_.size()); }

        /// 返回 instance-rate VAB 的 VkBuffer，供 vkCmdBindVertexBuffers 使用
        VkBuffer GetVABBuffer() const { return tile_vab_buf_; }

        /// 返回 indirect draw buffer，供 vkCmdDrawIndirect 使用
        hgl::graph::IndirectDrawBuffer* GetIndirectBuffer() const { return indirect_buf_; }
    };

}  // namespace hgl::ecs
