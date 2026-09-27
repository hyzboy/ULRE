#pragma once

#include <hgl/CoreType.h>
#include <cstdint>

namespace hgl::graph
{
    /**
     * 全局地址表（Scene 集 Set 0，当前 binding=2；整集退场后改由 pc_root 寻址）。
     *
     * 存放全局池与长期有效数据的 64 位 BDA 设备地址。
     * 一次写入、持久有效，着色器按需解引用。
     *
     * 地址归口口径：**静态 / 长期有效**的地址进本表（材质私有池、渲染项表、
     * 相机行表、调色板）；**每 pass / 每帧变化**的地址走 pc_root push constant。
     */
    struct GlobalAddresses
    {
        uint64_t addr_mesh_draw_params = 0;
        uint64_t addr_pbr_surface = 0;
        uint64_t addr_emissive_surface = 0;
        uint64_t addr_transmission_surface = 0;
        uint64_t addr_global_render_items = 0;
        uint64_t addr_draw_item_ids = 0;
        uint64_t addr_camera_info = 0;
        uint64_t addr_color_palette = 0;
    };

    static_assert(sizeof(GlobalAddresses) == 64, "GlobalAddresses 必须为 64 字节（8 个 uint64_t）");
}
