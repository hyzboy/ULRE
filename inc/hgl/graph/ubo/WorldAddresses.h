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
     *   **本表** = **世界私有的观察者 / 状态**（相机行表、实体行表、实体 4-ID 表、L2W、光源行表、影子数据、
     *   环境光块；sky / shadow / env 随 C2 迁入，光源行表随 T6b 迁入；
     *   **`addr_sky` 已于 2026-10-07 随 sky 段整删退出本表**）；
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
        uint64_t addr_shadow = 0;               // **本世界**的影子数据 ShadowInfo（T6b/P4：已世界私有，不再是 profile 的；
                                                // 每帧槽一份，下标 = 帧槽；写者 = MainLightShadowSystem，设备写入点 =
                                                // ECSContext::CommitShadowInfo（交换链帧 acquire 后写本帧槽））
        uint64_t addr_entity_gpu = 0;           // 实体行表 EntityGPU（世界私有；T3：GPU 剔除的枚举源，按 EntityID.index 索引）
        uint64_t addr_l2w = 0;                  // L2W 矩阵池（世界私有 TransformAssignmentBuffer；按帧槽切份，每帧槽一份）
        uint64_t addr_entity_render_ids = 0;    // 实体 4-ID 表（世界私有 EntityRenderIDTable，按 EntityID.index 直索引，
                                                // 行 = graph::RenderItemDescriptor 16 B；T3d-3a：GPU 消费者取**资源池口径** id 的唯一通路
                                                // —— `EntityGPU.row[Geometry]` 是组件池行号，与 AABB / MeshDrawParams 全局池的
                                                // asset geometry_id **不同口径**，剔除与 gather 必须读本表）
        uint64_t addr_lights = 0;               // 光源行表 LightRow（世界私有 LightRowTable；**行 0 = 主方向光**；
                                                // T6b：光源定义自 SkyInfo 迁出后的唯一真源。按帧槽切份 ——
                                                // 光参数可由作者逐帧改（太阳方向在示例里就是逐帧写的）
        uint64_t addr_ambient_env = 0;          // 环境光 provider 参数块 AmbientEnvInfo（世界私有；T1 归位）——
                                                // 内容 = IBL 三个 bindless 句柄，资源就绪时写一次、无人逐帧改
                                                // ⇒ 单份 buffer（全帧槽同址；逐帧演化的块须按帧槽分份，见 R3/T2）
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
