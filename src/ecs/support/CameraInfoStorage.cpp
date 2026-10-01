#include <hgl/ecs/support/CameraInfoStorage.h>

#include <hgl/vk/VKDevice.h>
#include <hgl/log/Log.h>

#include <cstdio>
#include <cstring>

namespace hgl::ecs
{
    bool CameraInfoStorage::Create(graph::VulkanDevice *device, const std::string &world_name)
    {
        if (pool.GetBuffer())
            return true;   // 已建

        if (!device)
        {
            GLogError("[CameraInfoStorage] Create rejected: device is null");
            return false;
        }

        char name_buf[160];
        std::snprintf(name_buf, sizeof(name_buf), "World:%s:CameraInfo", world_name.c_str());
        const AnsiString pool_name(name_buf);

        // 行号完全由调用方算术划分（camera_slot × frame_slot）⇒ reserve_rows = 0，
        // 建后**整块预激活**，CommitRow 的行校验恒成立（写入不会被静默拒绝）。
        // 相机行只走 BDA，不经 recipe ssbo_id ⇒ 传 0。
        if (!pool.Create(device, pool_name, sizeof(graph::CameraInfo), kRowCount, 0u, 0u))
        {
            GLogError("[CameraInfoStorage] pool creation failed: %s (row_bytes=%u rows=%u)",
                      pool_name.c_str(),
                      static_cast<uint32_t>(sizeof(graph::CameraInfo)),
                      kRowCount);
            return false;
        }

        if (!pool.ActivateAllRows())
        {
            GLogError("[CameraInfoStorage] ActivateAllRows failed (rows=%u)", kRowCount);
            Reset();
            return false;
        }

        return true;
    }

    void CameraInfoStorage::Reset()
    {
        pool.Reset();

        // 槽账目随行池一起清空：世界的相机全部失去槽（下次三级解析重新认领）。
        for (uint32_t i = 0; i < kSlotCapacity; ++i)
            slot_owners[i].reset();
    }

    uint32_t CameraInfoStorage::AcquireCameraSlot(const std::shared_ptr<CameraComponent> &owner)
    {
        if (!owner)
        {
            GLogError("[CameraInfoStorage] AcquireCameraSlot 拒绝空宿主：槽的宿主必须由 shared_ptr 持有"
                      "（槽占用期 = 宿主生命期）");
            return INVALID_SLOT;
        }

        // 0 号槽恒留给本世界默认相机（含强制 fallback 相机），不参与分配。
        // 「宿主已死（expired）」= 空闲槽 ⇒ 自然复用，无需任何归还挂钩。
        for (uint32_t slot = kDefaultCameraSlot + 1u; slot < kSlotCapacity; ++slot)
        {
            if (slot_owners[slot].expired())
            {
                slot_owners[slot] = owner;
                return slot;
            }
        }

        GLogError("[CameraInfoStorage] 相机槽耗尽：上限 %u 槽（%u 行 / %u 帧槽）——容量不扩容，请检查是否有相机未归还",
                  kSlotCapacity, kRowCount, kFrameSlotCount);
        return INVALID_SLOT;
    }

    bool CameraInfoStorage::BindCameraSlot(const uint32_t slot, const std::shared_ptr<CameraComponent> &owner)
    {
        if (!IsValidSlot(slot) || !owner)
        {
            GLogError("[CameraInfoStorage] BindCameraSlot 拒绝：slot=%u owner=%p（0..%u 有效，宿主不得为空）",
                      slot, static_cast<const void *>(owner.get()), kSlotCapacity - 1u);
            return false;
        }

        // 0 号槽唯一：指派即顶替原宿主（默认相机换人 = 一行赋值，不需要任何显式释放）。
        slot_owners[slot] = owner;
        return true;
    }

    bool CameraInfoStorage::ReleaseCameraSlot(const uint32_t slot)
    {
        if (slot == kDefaultCameraSlot || slot >= kSlotCapacity)
        {
            GLogError("[CameraInfoStorage] ReleaseCameraSlot 拒绝无效槽：%u（1..%u 有效；0 号槽=默认相机）",
                      slot, kSlotCapacity - 1u);
            return false;
        }

        if (slot_owners[slot].expired())
            return false;   // 宿主已死 / 从未分配：槽早已空出，重复归还不改变账目

        slot_owners[slot].reset();
        return true;
    }

    bool CameraInfoStorage::ReleaseCameraSlot(const CameraComponent *camera)
    {
        if (!camera)
            return false;

        const uint32_t slot = FindSlot(camera);
        if (slot == INVALID_SLOT)
            return false;

        return ReleaseCameraSlot(slot);
    }

    CameraComponent *CameraInfoStorage::GetSlotOwner(const uint32_t slot) const
    {
        if (!IsValidSlot(slot))
            return nullptr;

        return slot_owners[slot].lock().get();
    }

    std::shared_ptr<CameraComponent> CameraInfoStorage::GetSlotOwnerShared(const uint32_t slot) const
    {
        if (!IsValidSlot(slot))
            return {};

        return slot_owners[slot].lock();
    }

    uint32_t CameraInfoStorage::FindSlot(const CameraComponent *camera) const
    {
        if (!camera)
            return INVALID_SLOT;

        for (uint32_t slot = 0; slot < kSlotCapacity; ++slot)
        {
            if (slot_owners[slot].lock().get() == camera)
                return slot;
        }

        return INVALID_SLOT;
    }

    bool CameraInfoStorage::IsSlotUsed(const uint32_t slot) const
    {
        return IsValidSlot(slot) && !slot_owners[slot].expired();
    }

    bool CameraInfoStorage::WriteCameraRow(const uint32_t camera_slot,
                                           const uint32_t frame_slot,
                                           const graph::CameraInfo &info)
    {
        if (camera_slot >= kSlotCapacity || frame_slot >= kFrameSlotCount)
        {
            GLogError("[CameraInfoStorage] WriteCameraRow 越界：camera_slot=%u frame_slot=%u（上限 %u x %u）",
                      camera_slot, frame_slot, kSlotCapacity, kFrameSlotCount);
            return false;
        }

        const uint32_t row = CameraRow(camera_slot, frame_slot);

        void *dst = pool.RowCPU(row);
        if (!dst || pool.GetRowBytes() < sizeof(graph::CameraInfo))
        {
            GLogError("[CameraInfoStorage] WriteCameraRow 取不到行内存：row=%u", row);
            return false;
        }

        std::memcpy(dst, &info, sizeof(graph::CameraInfo));
        return pool.CommitRow(row);
    }

    const graph::CameraInfo *CameraInfoStorage::GetCameraRow(const uint32_t camera_slot,
                                                             const uint32_t frame_slot) const
    {
        if (camera_slot >= kSlotCapacity || frame_slot >= kFrameSlotCount)
            return nullptr;

        const uint32_t row = CameraRow(camera_slot, frame_slot);

        return static_cast<const graph::CameraInfo *>(pool.RowCPU(row));
    }
}//namespace hgl::ecs
