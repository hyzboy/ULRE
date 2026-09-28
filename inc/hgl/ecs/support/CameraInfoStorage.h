#pragma once

#include <hgl/graph/CameraInfo.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/vk/buffer/ActiveRowPool.h>
#include <hgl/type/String.h>

#include <cstdint>
#include <string>

namespace hgl::graph
{
    class VulkanDevice;
}

namespace hgl::ecs
{
    /**
     * 世界私有相机行存储（相机是**世界级**观察者数据，不是设备级资源池成员）。
     *
     * 定稿见 doc/world-addresses-and-camera-model-plan.md §2/§3：
     * - 行号 = `camera_slot * kFrameSlotCount + frame_slot`；shader 侧仍由 `pc_root.camera_row` 索引，
     *   地址由**世界表**（WorldAddresses.addr_camera_info）给出；
     * - **槽 0 恒为本世界默认相机**（三级解析的落点）；槽 1..kSlotCapacity-1 由本类分配器给出
     *   （普通相机 / 灯光相机 / 镜子相机 / 系统内建相机），**拥有者负责归还**；
     * - 容量 16 槽 × HGL_FRAME_SLOT_TOTAL 帧槽 = 128 行；超限报错返回 INVALID_SLOT，**不扩容**。
     *
     * 为什么行空间整块预激活：行号完全由调用方算术决定、不经 Acquire/Release，
     * 预激活使 `CommitRow` 的行校验恒成立（写入不会被静默拒绝）——沿用 GlobalSSBOBufferRegistry
     * 里 CameraInfo 池的口径。
     */
    class CameraInfoStorage
    {
    public:
        static constexpr uint32_t kSlotCapacity      = 16u;                              ///< 世界相机槽上限（fail-fast）
        static constexpr uint32_t kFrameSlotCount    = HGL_FRAME_SLOT_TOTAL;             ///< per-frame 帧槽数
        static constexpr uint32_t kRowCount          = kSlotCapacity * kFrameSlotCount;  ///< 行空间（= 128）
        static constexpr uint32_t kDefaultCameraSlot = 0u;                               ///< 0 号槽 = 本世界默认相机专属
        static constexpr uint32_t INVALID_SLOT       = UINT32_MAX;

        /// 行号 = 相机槽 × 帧槽总数 + 帧槽
        static constexpr uint32_t CameraRow(const uint32_t camera_slot, const uint32_t frame_slot)
        {
            return camera_slot * kFrameSlotCount + frame_slot;
        }

    public:

        /// 建世界私有行池（HOST_VISIBLE 持久映射 + BDA 行地址）。world_name 用于 buffer 命名（多世界可辨）。
        bool Create(graph::VulkanDevice *device, const std::string &world_name);

        /// 释放（回到未创建状态）。
        void Reset();

        bool IsReady() const { return pool.IsReady(); }

        /// 分配相机槽（0 号槽不可分配）。耗尽 ⇒ 报错 + INVALID_SLOT（容量不扩容）。
        uint32_t AcquireCameraSlot();

        /// 归还相机槽（0 号槽拒绝）。
        bool ReleaseCameraSlot(uint32_t slot);

        bool IsSlotUsed(uint32_t slot) const;

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
        bool slot_used[kSlotCapacity] = {};
    };
}//namespace hgl::ecs
