#pragma once

#include <hgl/CoreType.h>
#include <hgl/common/RenderOptions.h>
#include <cstdint>

namespace hgl::graph
{
    /**
     * 世界地址表（**无绑定无集**）：世界私有的 SSBO 地址表，基址经 pc_root.addr_world_addresses 下发
     *（GLSL：#define world_addresses WorldAddressesRef(pc_root.addr_world_addresses)）。
     *
     * 分层（定稿见 doc/world-addresses-and-camera-model-plan.md §1）：
     *   `GlobalAddresses` = **跨世界共享的资源池**（MeshDrawParams 池 / 材质行池 / 调色板 / viewport）；
     *   **本表** = **世界私有的观察者 / 状态**（相机行表、渲染项表、DrawItemID 表；sky / shadow / env 随 C2 迁入）；
     *   `pc_root` = 每批 / 每材质 / 本字体。
     *
     * 为什么必须独立成表：这些 buffer 由**世界**（`ECSContext`）拥有，一个设备上可以同时存在多个世界
     *（主世界 + `OffscreenWorld` 子世界）。把它们放进全局表只能表达"最后一个世界"的地址
     * ⇒ 多世界同帧互踩（历史事故：L2W 曾注册进设备级全局域，第二个世界注册时静默顶掉第一个）。
     *
     * 表按 **HGL_FRAME_SLOT_TOTAL 多份**（每帧只写本帧槽），pc_root 随之指向本帧槽的表地址，
     * 与 GlobalAddresses 的槽机制同形（槽地址 = 基址 + 槽号 × 步长）。
     */
    struct WorldAddresses
    {
        uint64_t addr_camera_info = 0;          // 相机行表（世界私有；行号 = 相机槽 × 帧槽总数 + 帧槽）
        uint64_t addr_global_render_items = 0;  // 4-ID 渲染项表（世界私有 RenderItemDataStorage）
        uint64_t addr_draw_item_ids = 0;        // DrawItemID 压缩索引表（世界私有 DrawItemIDStorage）

        // C2（Env 归世界）将迁入：addr_sky / addr_shadow / addr_env（随世界 profile 物化）。
    };

    /// 表槽数 = per-frame 槽总数（每帧写自己那一槽）。
    inline constexpr uint32_t kWorldAddressesSlotCount = HGL_FRAME_SLOT_TOTAL;

    /// 槽步长：与 GlobalAddresses 同形（**上界**，不是 sizeof；16B 对齐 + 留余量）。
    inline constexpr uint32_t kWorldAddressesSlotStride = 128;

    static_assert(sizeof(WorldAddresses) <= kWorldAddressesSlotStride,
                  "WorldAddresses 超过单槽容量（改 kWorldAddressesSlotStride 并同步 GLSL 块）");
    static_assert(kWorldAddressesSlotStride % 16 == 0,
                  "槽步长必须 16B 对齐（buffer_reference_align=16 的取址要求）");

    /// 相机行号 → 帧槽号：行号 = 相机槽 × 槽总数 + 槽（等价于 row % 槽总数）。
    inline constexpr uint32_t WorldAddressesSlotFromCameraRow(const uint32_t camera_row)
    {
        return camera_row % kWorldAddressesSlotCount;
    }
}
