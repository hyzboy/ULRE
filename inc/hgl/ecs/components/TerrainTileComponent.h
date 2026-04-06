#pragma once

#include <hgl/ecs/core/Component.h>
#include <cstdint>

namespace hgl::graph
{
    class ShaderProgram;
    class MaterialInstance;
    class GraphicsPipeline;
}

namespace hgl::ecs
{
    /**
     * TerrainTileComponent — 无 VBO/IBO 地形 Tile 组件
     *
     * 每个 Tile 由 grid_width × grid_height 个 Quad 组成。
     * 顶点位置完全由 Vertex Shader 根据 gl_VertexIndex / gl_InstanceIndex 计算，
     * Z 分量从高度图采样；因此无需任何顶点/索引缓冲。
     *
     * 渲染方式：所有 Tile 合并到一次 vkCmdDrawIndirect 调用。
     *   - Instance-rate VAB：每实例存储 (tile_x, tile_y, grid_width, grid_height)
     *   - IndirectDrawBuffer：每 Tile 一条 VkDrawIndirectCommand
     *   - VS 由 gl_VertexIndex 计算局部顶点 XY，由 gl_InstanceIndex 索引 Tile 参数
     *
     * 所有 Tile 共享同一 pipeline 与 material；不同 material 需要独立管线组。
     */
    struct TerrainTileComponent : public Component
    {
        int32_t  tile_x      = 0;   ///< 世界坐标 Tile X 索引
        int32_t  tile_y      = 0;   ///< 世界坐标 Tile Y 索引
        uint32_t grid_width  = 32;  ///< 单 Tile X 方向 Quad 数量
        uint32_t grid_height = 32;  ///< 单 Tile Y 方向 Quad 数量

        hgl::graph::ShaderProgram*         material  = nullptr;  ///< Vulkan 材质（含高度图绑定，不持有）
        hgl::graph::MaterialInstance* mat_inst  = nullptr;  ///< 材质实例（不持有）
        hgl::graph::GraphicsPipeline*         pipeline  = nullptr;  ///< Vulkan 管线（不持有；全体 Tile 共享）

        bool visible = true;  ///< 是否参与渲染

        /// 此 Tile 的顶点数量 = grid_width * grid_height * 6（每 Quad 两个三角形，无 IBO）
        uint32_t VertexCount() const { return grid_width * grid_height * 6; }

    public:
        explicit TerrainTileComponent(const std::string& name = "TerrainTile")
            : Component(name) {}

        ~TerrainTileComponent() override = default;

        const char* GetSystemGroupName() const { return "Terrain"; }
    };

}  // namespace hgl::ecs
