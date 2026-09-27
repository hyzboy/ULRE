#pragma once

#include <hgl/CoreType.h>
#include <cstdint>

namespace hgl::graph
{
    /**
     * 全局地址表（**无绑定无集**）：本表是 SSBO，基址经 pc_root.addr_global_addresses 下发
     *（GLSL：#define global_addresses GlobalAddressesRef(pc_root.addr_global_addresses)），
     * 表内字段即「全局 / 长期有效」数据的 64 位 BDA 设备地址，着色器按需解引用。
     *
     * 一次写入、持久有效。sky / viewport / shadow 的地址迁入本表时，表按
     * HGL_FRAME_SLOT_TOTAL 多份（每帧只写本帧槽），pc_root 随之指向本帧槽的表基址。
     *
     * 地址归口口径：**全局 / 长期有效**的地址进本表（材质私有池、渲染项表、相机行表、
     * 调色板、天空 / 视口 / 阴影）；**每批 / 每材质 / 本字体**的地址随 pc_root 走
     *（同一帧内逐批不同，一张标量表装不下，见 RootAddressPush.h）。
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
