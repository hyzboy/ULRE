#pragma once

#include <hgl/CoreType.h>
#include <hgl/common/RenderOptions.h>
#include <cstdint>

namespace hgl::graph
{
    /**
     * 全局地址表（**无绑定无集**）：本表是 SSBO，基址经 pc_root.addr_global_addresses 下发
     *（GLSL：#define global_addresses GlobalAddressesRef(pc_root.addr_global_addresses)），
     * 表内字段即「全局 / 长期有效」数据的 64 位 BDA 设备地址，着色器按需解引用。
     *
     * 表按 **HGL_FRAME_SLOT_TOTAL 多份**（每帧只写本帧槽），pc_root 随之指向本帧槽的表地址。
     * 为什么必须多份：绑定时代「每帧换绑定」由命令缓冲自带时序；地址进了共享表之后，
     * 覆盖写会踩到仍在执行的在途帧 —— 按槽分份后每帧只写自己那一槽，回到无竞争。
     *
     * 地址归口口径：**全局 / 长期有效**的地址进本表（材质私有池、渲染项表、相机行表、
     * 调色板、天空 / 视口 / 阴影）；**每批 / 每材质 / 本字体**的地址随 pc_root 走
     *（同一帧内逐批不同，一张标量表装不下，见 RootAddressPush.h）。
     */
    struct GlobalAddresses
    {
        // ── 全局字段：对所有帧槽相同（池基址 / 长期有效数据的地址）──
        uint64_t addr_mesh_draw_params = 0;
        uint64_t addr_pbr_surface = 0;
        uint64_t addr_emissive_surface = 0;
        uint64_t addr_transmission_surface = 0;
        uint64_t addr_global_render_items = 0;
        uint64_t addr_draw_item_ids = 0;
        uint64_t addr_camera_info = 0;
        uint64_t addr_color_palette = 0;

        // ── 每帧槽字段：地址随帧槽变化（sky 的 buffer 每帧不同）──
        uint64_t addr_sky = 0;
    };

    /// 表槽数 = per-frame 槽总数（每帧写自己那一槽，见文件头）。
    inline constexpr uint32_t kGlobalAddressesSlotCount = HGL_FRAME_SLOT_TOTAL;

    /// 槽步长：**上界**，不是 sizeof —— 表内每个槽占这么多字节，槽地址 = 基址 + 槽号*步长。
    /// 取整到 16B 对齐（buffer_reference_align=16）；留出余量，后续往表里加字段不用改步长。
    inline constexpr uint32_t kGlobalAddressesSlotStride = 128;

    static_assert(sizeof(GlobalAddresses) <= kGlobalAddressesSlotStride,
                  "GlobalAddresses 超过单槽容量（改 kGlobalAddressesSlotStride 并同步 GLSL 块）");
    static_assert(kGlobalAddressesSlotStride % 16 == 0,
                  "槽步长必须 16B 对齐（buffer_reference_align=16 的取址要求）");

    /// 相机行号 → 帧槽号：行号 = camera_id * 槽总数 + 槽（GlobalSSBOBufferRegistry::CameraRow）。
    /// push pc_root 时用它把「本批次所属帧槽」翻成表地址。
    inline constexpr uint32_t GlobalAddressesSlotFromCameraRow(const uint32_t camera_row)
    {
        return camera_row % kGlobalAddressesSlotCount;
    }
}
