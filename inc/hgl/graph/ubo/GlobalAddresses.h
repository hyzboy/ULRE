#pragma once

#include <hgl/CoreType.h>
#include <cstdint>

namespace hgl::graph
{
    /**
     * 全局地址 UBO（对应 Set 0 Binding 4）。
     *
     * 存放全局池的 64 位 BDA 设备地址。
     * 一次写入、持久有效，着色器按需解引用。
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
    };

    static_assert(sizeof(GlobalAddresses) == 56, "GlobalAddresses 必须为 56 字节（7 个 uint64_t）");
}
