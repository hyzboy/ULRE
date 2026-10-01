#pragma once

#include <hgl/graph/CameraInfo.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/ecs/components/CameraComponent.h>
#include <hgl/vk/buffer/ActiveRowPool.h>
#include <hgl/type/String.h>

#include <cstdint>
#include <memory>
#include <string>

namespace hgl::graph
{
    class VulkanDevice;
}

namespace hgl::ecs
{
    /**
     * 世界私有相机表（相机 = **世界级资源**：既不是设备级资源池成员，也不是组件自己的状态）。
     *
     * 定稿见 doc/world-addresses-and-camera-model-plan.md §2/§3 与
     * doc/future/ULRE_FINAL_TARGET_v2_设计约束.md §9（A6：`CameraComponent` 只描述**参数**，
     * 槽位/资源归世界级）：
     * - 行号 = `camera_slot * kFrameSlotCount + frame_slot`；shader 侧仍由 `pc_root.camera_row` 索引，
     *   地址由**世界表**（WorldAddresses.addr_camera_info）给出；
     * - 容量 16 槽 × HGL_FRAME_SLOT_TOTAL 帧槽 = 128 行；超限报错返回 INVALID_SLOT，**不扩容**。
     *
     * **槽账目（唯一真源）= `slot_owners[slot]` 弱引用：宿主生命期即槽占用期。**
     * - 0 号槽 = 本世界默认相机（三级解析的落点，含常驻 fallback），由 `ECSContext::SetDefaultCamera` 写入；
     * - 1..15 由 `AcquireCameraSlot()` 分配给普通相机 / 灯光相机 / 镜子相机 / 系统内建相机；
     * - 「无宿主 / 宿主已死」= 该槽空闲 —— 因此**不存在**第二份 `slot_used` 记账，也**不需要**
     *   「相机销毁时通知世界归还」的挂钩：相机（实体组件 / 作者 shared_ptr / CameraSlotGuard 所持）
     *   先死时弱引用自动失效，槽在下一次申请时即可复用；世界先死时表随世界销毁，
     *   宿主侧没有任何裸指针可悬垂（历史教训：注册表 + Detach 方案在「世界从未 Initialize」
     *   时会留下悬垂指针）。
     *
     * 为什么行空间整块预激活：行号完全由调用方算术决定、不经 Acquire/Release，
     * 预激活使 `CommitRow` 的行校验恒成立（写入不会被静默拒绝）——沿用 GlobalSSBOBufferRegistry
     * 里 CameraInfo 池的口径。
     */
    class CameraInfoStorage
    {
    public:
        // 槽形状的**唯一真源**在 `CameraComponent`（槽语义定义处）——这里只做别名 + parity 断言，
        // 避免两个常量各写一份而漂移。
        static constexpr uint32_t kSlotCapacity      = CameraComponent::kSlotCapacity;   ///< 世界相机槽上限（fail-fast）
        static constexpr uint32_t kFrameSlotCount    = HGL_FRAME_SLOT_TOTAL;             ///< per-frame 帧槽数
        static constexpr uint32_t kRowCount          = kSlotCapacity * kFrameSlotCount;  ///< 行空间（= 128）
        static constexpr uint32_t kDefaultCameraSlot = CameraComponent::kDefaultSlot;    ///< 0 号槽 = 本世界默认相机专属
        static constexpr uint32_t INVALID_SLOT       = CameraComponent::kInvalidSlot;    ///< 未分配（≠ 0）

        static_assert(kSlotCapacity == 16u, "世界相机槽容量必须是 16（doc/world-addresses-and-camera-model-plan.md §0.6）");
        static_assert(kDefaultCameraSlot == 0u, "0 号槽 = 本世界默认相机");
        static_assert(INVALID_SLOT >= kSlotCapacity, "未分配哨兵不得落在合法槽号区间内");

        /// 合法槽号判定（写入 / 读取 / 行号算术的统一入口）
        static constexpr bool IsValidSlot(const uint32_t slot) { return slot < kSlotCapacity; }

        /// 行号 = 相机槽 × 帧槽总数 + 帧槽
        static constexpr uint32_t CameraRow(const uint32_t camera_slot, const uint32_t frame_slot)
        {
            return camera_slot * kFrameSlotCount + frame_slot;
        }

    public:

        /// 建世界私有行池（HOST_VISIBLE 持久映射 + BDA 行地址）。world_name 用于 buffer 命名（多世界可辨）。
        /// @note 槽账目（`slot_owners`）与行池无关：没有设备（未 Create）时账目照样可用。
        bool Create(graph::VulkanDevice *device, const std::string &world_name);

        /// 释放行池并**清空槽账目**（回到未创建状态）：世界的全部相机随之失去槽，下次解析重新认领。
        void Reset();

        bool IsReady() const { return pool.IsReady(); }

        // ==== 槽账目（世界级相机表的唯一真源）====

        /// 申请一个**普通**相机槽（0 号槽不参与分配）：优先复用「宿主已死」的槽；
        /// 容量 16 用满 ⇒ 报错 + INVALID_SLOT（不扩容 = fail-fast）。
        /// @param owner 槽的宿主：**必须由 shared_ptr 持有**（弱引用即槽占用期的判据）
        uint32_t AcquireCameraSlot(const std::shared_ptr<CameraComponent> &owner);

        /// 直接把某槽指派给 owner（不申请）：0 号槽 = 本世界默认相机专属；
        /// 越界 / owner 为空 ⇒ 拒绝。0 号槽唯一 ⇒ 原宿主自动被顶替。
        bool BindCameraSlot(uint32_t slot, const std::shared_ptr<CameraComponent> &owner);

        /// 归还槽：0 号槽 / 越界 / 宿主已死（槽早已空出）⇒ false（幂等，重复归还不改变账目）。
        bool ReleaseCameraSlot(uint32_t slot);

        /// 按相机反查并归还它的槽（不属于本世界 / 无槽 ⇒ false）。
        bool ReleaseCameraSlot(const CameraComponent *camera);

        /// 槽的宿主相机；槽空闲 / 宿主已死 ⇒ nullptr（0 号槽 = 本世界默认相机）
        CameraComponent *GetSlotOwner(uint32_t slot) const;

        /// 槽的宿主相机（shared 形式；槽空闲 / 宿主已死 ⇒ 空）
        std::shared_ptr<CameraComponent> GetSlotOwnerShared(uint32_t slot) const;

        /// 相机在本世界的槽号；不属于本世界 ⇒ INVALID_SLOT
        uint32_t FindSlot(const CameraComponent *camera) const;

        /// 槽是否被占用（= 宿主存活）
        bool IsSlotUsed(uint32_t slot) const;

        // ==== 相机行 ====

        /// 写入「某相机槽 × 某帧槽」的行（越界 ⇒ 报错返回 false）。
        bool WriteCameraRow(uint32_t camera_slot, uint32_t frame_slot, const graph::CameraInfo &info);

        /// CPU 侧读本世界某相机某帧槽的行（剔除 / gizmo / 日志用；越界返回 nullptr）。
        const graph::CameraInfo *GetCameraRow(uint32_t camera_slot, uint32_t frame_slot) const;

        /// 本世界相机行表的设备基址（写入 WorldAddresses.addr_camera_info）。
        uint64_t GetGPUBase() const { return pool.GetGPUBase(); }

        graph::ActiveRowPool       &GetPool()       { return pool; }
        const graph::ActiveRowPool &GetPool() const { return pool; }

        /// 本池 CommitRow 被拒次数（契约判据：正常运行恒 0）。
        uint64_t GetCommitRejectCount() const { return pool.GetCommitRejectCount(); }

    private:

        graph::ActiveRowPool pool;

        /// 槽 → 宿主（弱引用）：**唯一真源**；空 / 过期 = 空闲
        std::weak_ptr<CameraComponent> slot_owners[kSlotCapacity];
    };
}//namespace hgl::ecs
